// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use std::path::Path;
use std::process::Command;
use std::thread::sleep;
use std::time::Duration;

#[derive(Debug)]
pub struct SoundError(pub String);

impl std::fmt::Display for SoundError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "som: {}", self.0)
    }
}

pub fn hdmi_play(wav: &str) -> Result<(), SoundError> {
    if !Path::new(wav).exists() {
        return Err(SoundError(format!("wav ausente: {wav}")));
    }
    let st = Command::new("aplay")
        .args(["-D", "plughw:0,0", wav])
        .status()
        .map_err(|e| SoundError(format!("aplay spawn: {e}")))?;
    if st.success() {
        Ok(())
    } else {
        Err(SoundError(format!("aplay falhou ({st}) p/ {wav}")))
    }
}

fn pwm_write(p: &str, v: &str) -> std::io::Result<()> {
    std::fs::write(p, v)
}

pub fn pwm_pulses(n: u32, freq_hz: u32, dur_ms: u64, gap_ms: u64) -> Result<(), SoundError> {
    let base = "/sys/class/pwm/pwmchip0";
    if !Path::new(&format!("{base}/pwmchip0")).exists()
        && !Path::new(&format!("{base}/pwm0")).exists()
    {
        if !Path::new(&format!("{base}/pwm0")).exists() {
            pwm_write(&format!("{base}/export"), "0")
                .map_err(|e| SoundError(format!("pwm export: {e}")))?;
            sleep(Duration::from_millis(120));
        }
    }
    if !Path::new(&format!("{base}/pwm0")).exists() {
        return Err(SoundError(
            "pwm0 ausente (dtoverlay=pwm,pin=18,func=2?)".into(),
        ));
    }
    let period: u32 = 1_000_000_000 / freq_hz.max(1);
    pwm_write(&format!("{base}/pwm0/period"), &period.to_string())
        .map_err(|e| SoundError(format!("pwm period: {e}")))?;
    pwm_write(
        &format!("{base}/pwm0/duty_cycle"),
        &(period / 2).to_string(),
    )
    .map_err(|e| SoundError(format!("pwm duty: {e}")))?;
    for i in 0..n {
        pwm_write(&format!("{base}/pwm0/enable"), "1")
            .map_err(|e| SoundError(format!("pwm enable: {e}")))?;
        sleep(Duration::from_millis(dur_ms));
        let _ = pwm_write(&format!("{base}/pwm0/enable"), "0");
        if i + 1 < n {
            sleep(Duration::from_millis(gap_ms));
        }
    }
    Ok(())
}

pub fn confirma(assets_dir: &str) -> Result<(), SoundError> {
    let wav = format!("{assets_dir}/confirma.wav");
    let h = hdmi_play(&wav);
    let p = pwm_pulses(2, 2000, 120, 180);
    match (h, p) {
        (Ok(()), _) | (_, Ok(())) => Ok(()),
        (Err(e1), Err(e2)) => Err(SoundError(format!("confirma sem som: {e1} + {e2}"))),
    }
}

pub fn fim(assets_dir: &str) -> Result<(), SoundError> {
    let wav = format!("{assets_dir}/fim.wav");
    let h = hdmi_play(&wav);
    let p = pwm_pulses(3, 2000, 250, 260);
    match (h, p) {
        (Ok(()), _) | (_, Ok(())) => Ok(()),
        (Err(e1), Err(e2)) => Err(SoundError(format!("fim sem som: {e1} + {e2}"))),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn wav_ausente_e_erro() {
        assert!(hdmi_play("/caminho/inexistente/xyz.wav").is_err());
    }

    #[test]
    fn pwm_sem_hw_e_erro() {
        if !Path::new("/sys/class/pwm/pwmchip0/pwm0").exists()
            && !Path::new("/sys/class/pwm/pwmchip0/export").exists()
        {
            assert!(pwm_pulses(1, 2000, 10, 10).is_err());
        }
    }
}
