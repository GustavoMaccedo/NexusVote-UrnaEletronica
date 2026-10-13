// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use argon2::password_hash::{PasswordHasher, PasswordVerifier};
use argon2::Argon2;
use password_hash::phc::PasswordHash;
use secrecy::{ExposeSecret, SecretBox};
use thiserror::Error;
use zeroize::ZeroizeOnDrop;

#[derive(Debug, Error)]
pub enum PinError {
    #[error("PIN fraco (mínimo 6 dígitos)")]
    Weak,
    #[error("formato PHC inválido")]
    BadPhc,
    #[error("argon2: {0}")]
    Argon(String),
    #[error("PIN incorreto")]
    Mismatch,
}

#[derive(ZeroizeOnDrop)]
pub struct Pin(SecretBox<String>);

impl Pin {
    pub fn new(s: String) -> Result<Self, PinError> {
        if s.trim().len() < 6 {
            return Err(PinError::Weak);
        }
        Ok(Self(SecretBox::new(Box::new(s))))
    }

    pub fn expose(&self) -> &str {
        self.0.expose_secret()
    }
}

pub fn hash_pin(pin: &Pin) -> Result<String, PinError> {
    Argon2::default()
        .hash_password(pin.expose().as_bytes())
        .map(|h| h.to_string())
        .map_err(|e| PinError::Argon(e.to_string()))
}

pub fn verify_pin(pin: &Pin, phc: &str) -> Result<(), PinError> {
    let parsed = PasswordHash::new(phc).map_err(|_| PinError::BadPhc)?;
    Argon2::default()
        .verify_password(pin.expose().as_bytes(), &parsed)
        .map_err(|_| PinError::Mismatch)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Role {
    Voter,
    PollWorker,
    Auditor,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hash_verify_ok_and_wrong() {
        let pin = Pin::new("492817".into()).unwrap();
        let phc = hash_pin(&pin).unwrap();
        assert!(phc.starts_with("$argon2id$"));
        assert!(verify_pin(&pin, &phc).is_ok());
        let wrong = Pin::new("000000".into()).unwrap();
        assert!(verify_pin(&wrong, &phc).is_err());
    }

    #[test]
    fn rejects_weak() {
        assert!(Pin::new("123".into()).is_err());
    }

    #[test]
    fn salts_differ() {
        let pin = Pin::new("492817".into()).unwrap();
        let a = hash_pin(&pin).unwrap();
        let b = hash_pin(&pin).unwrap();
        assert_ne!(a, b);
    }
}
