// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use std::fs::OpenOptions;
use std::io::{ErrorKind, Write};
use std::path::Path;

pub const PAPER_COLS: usize = 32;

#[derive(Debug)]
pub enum PrintErr {
    Ausente(String),
    Falha(String),
}

impl std::fmt::Display for PrintErr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PrintErr::Ausente(m) => write!(f, "IMPRESSORA-AUSENTE: {m}"),
            PrintErr::Falha(m) => write!(f, "FALHA-IMPRESSAO: {m}"),
        }
    }
}

pub fn printer_path() -> String {
    if let Ok(p) = std::env::var("NV_PRINTER_DEV") {
        if !p.trim().is_empty() {
            return p;
        }
    }
    if Path::new("/dev/usb/lp0").exists() {
        return "/dev/usb/lp0".into();
    }
    "/dev/usb/lp0".into()
}

fn latin1_byte(c: char) -> u8 {
    match c {
        '-' | '-' => return b'-',
        '“' | '”' => return b'"',
        '‘' | '’' => return b'\'',
        _ => {}
    }
    if (c as u32) < 256 {
        c as u8
    } else {
        b'?'
    }
}

fn latin1(s: &str) -> Vec<u8> {
    s.chars().map(latin1_byte).collect()
}

fn fit(s: &str) -> Vec<u8> {
    let b = latin1(s);
    if b.len() <= PAPER_COLS {
        b
    } else {
        b[..PAPER_COLS].to_vec()
    }
}

fn center(s: &str) -> Vec<u8> {
    let b = fit(s.trim());
    if b.len() >= PAPER_COLS {
        return b;
    }
    let pad = (PAPER_COLS - b.len()) / 2;
    let mut out = vec![b' '; pad];
    out.extend(b);
    out
}

pub fn build_job(title: &str, sessao: &str, linhas: &[String], qr_payload: &str) -> Vec<u8> {
    let mut j = vec![0x1B, 0x40];
    j.extend([0x1B, 0x74, 0x13]);
    j.extend([0x1B, 0x61, 0x01]);
    j.extend([0x1B, 0x45, 0x01]);
    j.extend(center("PROTOTIPO INDEPENDENTE"));
    j.push(0x0A);
    j.extend(center("USO EM LABORATORIO"));
    j.push(0x0A);
    j.extend(center("NAO OFICIAL - SEM VALOR"));
    j.push(0x0A);
    j.extend([0x1B, 0x45, 0x00]);
    j.extend([0x1B, 0x61, 0x01]);
    j.extend(center(title));
    j.push(0x0A);
    j.extend([0x1B, 0x61, 0x00]);
    j.extend(fit(&format!("SESSAO: {sessao}")));
    j.push(0x0A);
    j.extend(fit("SISTEMA: NEXUSVOTE-LAB"));
    j.push(0x0A);
    for l in linhas {
        j.extend(fit(l));
        j.push(0x0A);
    }
    j.extend([0x1B, 0x61, 0x01]);
    j.extend([0x1D, 0x28, 0x6B, 0x04, 0x00, 0x31, 0x41, 0x32, 0x00]);
    j.extend([0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x43, 0x05]);
    j.extend([0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x45, 0x31]);
    let d = qr_payload.as_bytes();
    let n = d.len() + 3;
    j.extend([
        0x1D,
        0x28,
        0x6B,
        (n % 256) as u8,
        (n / 256) as u8,
        0x31,
        0x50,
        0x30,
    ]);
    j.extend(d);
    j.extend([0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x51, 0x30]);
    j.extend([0x0A, 0x0A, 0x0A, 0x1D, 0x56, 0x42, 0x00]);
    j
}

pub fn print_raw(dev: &str, job: &[u8]) -> Result<(), PrintErr> {
    if !Path::new(dev).exists() {
        return Err(PrintErr::Ausente(format!("{dev} inexistente (cabo/usblp)")));
    }
    let mut f = OpenOptions::new()
        .write(true)
        .open(dev)
        .map_err(|e| match e.kind() {
            ErrorKind::NotFound => PrintErr::Ausente(format!("open {dev}: {e}")),
            ErrorKind::PermissionDenied => PrintErr::Ausente(format!("sem grupo lp p/ {dev}: {e}")),
            _ => PrintErr::Falha(format!("open {dev}: {e}")),
        })?;
    let mut off = 0;
    while off < job.len() {
        match f.write(&job[off..]) {
            Ok(0) => return Err(PrintErr::Falha("write retornou 0".into())),
            Ok(n) => off += n,
            Err(e) => return Err(PrintErr::Falha(format!("write @{off}: {e}"))),
        }
    }
    f.flush()
        .map_err(|e| PrintErr::Falha(format!("flush: {e}")))?;
    f.sync_all()
        .map_err(|e| PrintErr::Falha(format!("fsync: {e}")))?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn job_tem_init_qr_corte() {
        let job = build_job("ZERESIMA", "LAB-01", &["TOTAL: 0".into()], r#"{"s":"X"}"#);
        assert_eq!(&job[..2], &[0x1B, 0x40]);
        assert!(job
            .windows(8)
            .any(|w| w == [0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x51, 0x30]));
        assert_eq!(&job[job.len() - 4..], &[0x1D, 0x56, 0x42, 0x00]);
    }

    #[test]
    fn ausente_e_erro() {
        let r = print_raw("/dev/usb/lp-inexistente-xyz", &[0x0A]);
        assert!(matches!(r, Err(PrintErr::Ausente(_))));
    }

    #[test]
    fn latin1_ptbr() {
        assert_eq!(latin1("ACAO"), b"ACAO");
        assert_eq!(latin1_byte('-'), b'-');
    }
}
