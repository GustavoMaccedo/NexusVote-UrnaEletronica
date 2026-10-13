// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use std::collections::HashSet;
use std::fs::{File, OpenOptions};
use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum NonceError {
    #[error("formato de nonce inválido (esperado 32 hex)")]
    BadFormat,
    #[error("replay detectado")]
    Replay,
    #[error("io: {0}")]
    Io(String),
    #[error("limite de nonces excedido")]
    Overflow,
}

const MAX_NONCES: usize = 100_000;

pub fn valid_nonce_format(s: &str) -> bool {
    s.len() == 32 && s.bytes().all(|c| c.is_ascii_hexdigit())
}

#[derive(Debug, Default)]
pub struct MemNonceGuard {
    seen: HashSet<String>,
}

impl MemNonceGuard {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn accept(&mut self, nonce: &str) -> Result<(), NonceError> {
        if !valid_nonce_format(nonce) {
            return Err(NonceError::BadFormat);
        }
        if !self.seen.insert(nonce.to_lowercase()) {
            return Err(NonceError::Replay);
        }
        if self.seen.len() > MAX_NONCES {
            return Err(NonceError::Overflow);
        }
        Ok(())
    }
}

pub struct FileNonceGuard {
    mem: MemNonceGuard,
    path: PathBuf,
}

impl FileNonceGuard {
    pub fn open(path: &Path) -> Result<Self, NonceError> {
        let mut mem = MemNonceGuard::new();
        if path.exists() {
            let f = File::open(path).map_err(|e| NonceError::Io(e.to_string()))?;
            for line in BufReader::new(f).lines() {
                let line = line.map_err(|e| NonceError::Io(e.to_string()))?;
                let n = line.trim().to_lowercase();
                if !n.is_empty() {
                    if valid_nonce_format(&n) {
                        mem.seen.insert(n);
                    }
                }
            }
        }
        Ok(Self {
            mem,
            path: path.to_path_buf(),
        })
    }

    pub fn accept(&mut self, nonce: &str) -> Result<(), NonceError> {
        self.mem.accept(nonce)?;
        let mut f = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&self.path)
            .map_err(|e| NonceError::Io(e.to_string()))?;
        writeln!(f, "{}", nonce.to_lowercase()).map_err(|e| NonceError::Io(e.to_string()))?;
        f.sync_all().map_err(|e| NonceError::Io(e.to_string()))?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mem_rejects_replay_and_bad_format() {
        let mut g = MemNonceGuard::new();
        let n = "a".repeat(32);
        assert!(g.accept(&n).is_ok());
        assert!(matches!(g.accept(&n), Err(NonceError::Replay)));
        assert!(g.accept("curto").is_err());
        assert!(g.accept(&"z".repeat(32)).is_err());
    }

    #[test]
    fn file_survives_reopen() {
        let dir = std::env::temp_dir().join(format!("nv-nonce-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let p = dir.join("nonces.txt");
        let _ = std::fs::remove_file(&p);
        let n = "b".repeat(32);
        {
            let mut g = FileNonceGuard::open(&p).unwrap();
            g.accept(&n).unwrap();
        }
        {
            let mut g2 = FileNonceGuard::open(&p).unwrap();
            assert!(matches!(g2.accept(&n), Err(NonceError::Replay)));
        }
        let _ = std::fs::remove_file(&p);
    }
}
