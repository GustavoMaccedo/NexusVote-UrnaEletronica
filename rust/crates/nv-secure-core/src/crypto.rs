// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use aes_gcm::aead::{Aead, KeyInit};
use aes_gcm::{Aes256Gcm, Key, Nonce};
use chacha20poly1305::{ChaCha20Poly1305, Key as ChaKey, Nonce as ChaNonce};
use ed25519_dalek::{Signature, Signer, SigningKey, VerifyingKey};
use secrecy::{ExposeSecret, SecretBox};
use sha2::{Digest, Sha256};
use subtle::ConstantTimeEq;
use thiserror::Error;
use zeroize::{Zeroize, ZeroizeOnDrop};

#[derive(Debug, Error)]
pub enum CryptoError {
    #[error("tamanho inválido: esperado {expected}, obtido {got}")]
    BadLength { expected: usize, got: usize },
    #[error("assinatura inválida")]
    BadSignature,
    #[error("falha AEAD (tag/nonce)")]
    Aead,
    #[error("RNG do SO falhou")]
    Rng,
    #[error("hex inválido")]
    Hex,
}

pub fn sha256(data: &[u8]) -> [u8; 32] {
    let mut h = Sha256::new();
    h.update(data);
    h.finalize().into()
}

pub fn to_hex(bytes: &[u8]) -> String {
    const H: &[u8; 16] = b"0123456789abcdef";
    let mut s = String::with_capacity(bytes.len() * 2);
    for b in bytes {
        s.push(H[(b >> 4) as usize] as char);
        s.push(H[(b & 0xf) as usize] as char);
    }
    s
}

pub fn from_hex(s: &str) -> Result<Vec<u8>, CryptoError> {
    if s.len() % 2 != 0 {
        return Err(CryptoError::Hex);
    }
    let mut out = Vec::with_capacity(s.len() / 2);
    let b = s.as_bytes();
    for i in (0..b.len()).step_by(2) {
        let hi = hexval(b[i]).ok_or(CryptoError::Hex)?;
        let lo = hexval(b[i + 1]).ok_or(CryptoError::Hex)?;
        out.push((hi << 4) | lo);
    }
    Ok(out)
}

fn hexval(c: u8) -> Option<u8> {
    match c {
        b'0'..=b'9' => Some(c - b'0'),
        b'a'..=b'f' => Some(c - b'a' + 10),
        b'A'..=b'F' => Some(c - b'A' + 10),
        _ => None,
    }
}

pub fn ct_equal(a: &[u8], b: &[u8]) -> bool {
    a.ct_eq(b).into()
}

#[derive(ZeroizeOnDrop)]
pub struct Keypair {
    signing: SigningKey,
}

impl Keypair {
    pub fn generate() -> Result<Self, CryptoError> {
        let mut bytes = [0u8; 32];
        getrandom::fill(&mut bytes).map_err(|_| CryptoError::Rng)?;
        let signing = SigningKey::from_bytes(&bytes);
        bytes.zeroize();
        Ok(Self { signing })
    }

    pub fn from_secret_bytes(mut bytes: [u8; 32]) -> Self {
        let signing = SigningKey::from_bytes(&bytes);
        bytes.zeroize();
        Self { signing }
    }

    pub fn verifying_key(&self) -> VerifyingKey {
        self.signing.verifying_key()
    }

    pub fn verifying_hex(&self) -> String {
        to_hex(&self.signing.verifying_key().to_bytes())
    }

    pub fn sign(&self, msg: &[u8]) -> [u8; 64] {
        self.signing.sign(msg).to_bytes()
    }
}

pub type SecretBytes = SecretBox<[u8; 32]>;

pub fn verify_ed25519(pubkey: &[u8], msg: &[u8], sig: &[u8]) -> Result<(), CryptoError> {
    if pubkey.len() != 32 {
        return Err(CryptoError::BadLength {
            expected: 32,
            got: pubkey.len(),
        });
    }
    if sig.len() != 64 {
        return Err(CryptoError::BadLength {
            expected: 64,
            got: sig.len(),
        });
    }
    let mut pk = [0u8; 32];
    pk.copy_from_slice(pubkey);
    let mut sb = [0u8; 64];
    sb.copy_from_slice(sig);
    let vk = VerifyingKey::from_bytes(&pk).map_err(|_| CryptoError::BadSignature)?;
    let signature = Signature::try_from(&sb[..]).map_err(|_| CryptoError::BadSignature)?;
    vk.verify_strict(msg, &signature)
        .map_err(|_| CryptoError::BadSignature)
}

#[allow(deprecated)]
pub fn seal_aes_gcm(key: &SecretBytes, plaintext: &[u8]) -> Result<Vec<u8>, CryptoError> {
    let mut nonce_bytes = [0u8; 12];
    getrandom::fill(&mut nonce_bytes).map_err(|_| CryptoError::Rng)?;
    let cipher = Aes256Gcm::new(Key::<Aes256Gcm>::from_slice(key.expose_secret()));
    let ct = cipher
        .encrypt(Nonce::from_slice(&nonce_bytes), plaintext)
        .map_err(|_| CryptoError::Aead)?;
    let mut out = Vec::with_capacity(12 + ct.len());
    out.extend_from_slice(&nonce_bytes);
    out.extend_from_slice(&ct);
    Ok(out)
}

#[allow(deprecated)]
pub fn open_aes_gcm(key: &SecretBytes, blob: &[u8]) -> Result<Vec<u8>, CryptoError> {
    if blob.len() < 12 + 16 {
        return Err(CryptoError::Aead);
    }
    let cipher = Aes256Gcm::new(Key::<Aes256Gcm>::from_slice(key.expose_secret()));
    cipher
        .decrypt(Nonce::from_slice(&blob[..12]), &blob[12..])
        .map_err(|_| CryptoError::Aead)
}

#[allow(deprecated)]
pub fn seal_chacha(key: &SecretBytes, plaintext: &[u8]) -> Result<Vec<u8>, CryptoError> {
    let mut nonce_bytes = [0u8; 12];
    getrandom::fill(&mut nonce_bytes).map_err(|_| CryptoError::Rng)?;
    let cipher = ChaCha20Poly1305::new(ChaKey::from_slice(key.expose_secret()));
    let ct = cipher
        .encrypt(ChaNonce::from_slice(&nonce_bytes), plaintext)
        .map_err(|_| CryptoError::Aead)?;
    let mut out = Vec::with_capacity(12 + ct.len());
    out.extend_from_slice(&nonce_bytes);
    out.extend_from_slice(&ct);
    Ok(out)
}

#[allow(deprecated)]
pub fn open_chacha(key: &SecretBytes, blob: &[u8]) -> Result<Vec<u8>, CryptoError> {
    if blob.len() < 12 + 16 {
        return Err(CryptoError::Aead);
    }
    let cipher = ChaCha20Poly1305::new(ChaKey::from_slice(key.expose_secret()));
    cipher
        .decrypt(ChaNonce::from_slice(&blob[..12]), &blob[12..])
        .map_err(|_| CryptoError::Aead)
}

pub fn random_hex16() -> Result<String, CryptoError> {
    let mut b = [0u8; 16];
    getrandom::fill(&mut b).map_err(|_| CryptoError::Rng)?;
    Ok(to_hex(&b))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sha256_known() {
        assert_eq!(
            to_hex(&sha256(b"abc")),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );
    }

    #[test]
    fn ed25519_roundtrip_strict() {
        let kp = Keypair::generate().unwrap();
        let msg = b"boletim-canonico";
        let sig = kp.sign(msg);
        let pk = kp.verifying_key().to_bytes();
        assert!(verify_ed25519(&pk, msg, &sig).is_ok());
        let mut bad = sig;
        bad[0] ^= 1;
        assert!(verify_ed25519(&pk, msg, &bad).is_err());
    }

    #[test]
    fn aead_roundtrip() {
        let key = SecretBox::new(Box::new([7u8; 32]));
        let blob = seal_aes_gcm(&key, b"voto-secreto").unwrap();
        assert_eq!(open_aes_gcm(&key, &blob).unwrap(), b"voto-secreto");
        let mut tampered = blob.clone();
        let n = tampered.len();
        tampered[n - 1] ^= 1;
        assert!(open_aes_gcm(&key, &tampered).is_err());
    }

    #[test]
    fn chacha_roundtrip() {
        let key = SecretBox::new(Box::new([9u8; 32]));
        let blob = seal_chacha(&key, b"payload").unwrap();
        assert_eq!(open_chacha(&key, &blob).unwrap(), b"payload");
    }

    #[test]
    fn ct_eq_works() {
        assert!(ct_equal(b"abc", b"abc"));
        assert!(!ct_equal(b"abc", b"abd"));
        assert!(!ct_equal(b"abc", b"abcd"));
    }
}
