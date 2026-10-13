// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use crate::crypto::{sha256, to_hex};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum ChainError {
    #[error("cadeia vazia")]
    Empty,
    #[error("seq esperado {expected}, obtido {got}")]
    BadSeq { expected: u64, got: u64 },
    #[error("prev_hash divergente no seq {seq}")]
    BadPrev { seq: u64 },
    #[error("hash de linha divergente no seq {seq}")]
    BadHash { seq: u64 },
}

pub fn genesis_hash() -> [u8; 32] {
    sha256(b"nexusvote-lab-genesis-v1")
}

pub fn line_hash(prev: &[u8; 32], seq: u64, payload: &[u8]) -> [u8; 32] {
    let mut h = sha2::Sha256::new();
    use sha2::Digest;
    h.update(prev);
    h.update(seq.to_be_bytes());
    h.update(payload);
    h.finalize().into()
}

#[derive(Debug, Clone)]
pub struct ChainedLine {
    pub seq: u64,
    pub prev_hex: String,
    pub hash_hex: String,
    pub payload: Vec<u8>,
}

impl ChainedLine {
    pub fn next(prev: &[u8; 32], seq: u64, payload: Vec<u8>) -> Self {
        let h = line_hash(prev, seq, &payload);
        Self {
            seq,
            prev_hex: to_hex(prev),
            hash_hex: to_hex(&h),
            payload,
        }
    }
}

pub fn verify_chain(lines: &[ChainedLine]) -> Result<[u8; 32], ChainError> {
    if lines.is_empty() {
        return Err(ChainError::Empty);
    }
    let mut prev = genesis_hash();
    for (i, l) in lines.iter().enumerate() {
        let expected_seq = (i as u64) + 1;
        if l.seq != expected_seq {
            return Err(ChainError::BadSeq {
                expected: expected_seq,
                got: l.seq,
            });
        }
        if l.prev_hex != to_hex(&prev) {
            return Err(ChainError::BadPrev { seq: l.seq });
        }
        let h = line_hash(&prev, l.seq, &l.payload);
        if l.hash_hex != to_hex(&h) {
            return Err(ChainError::BadHash { seq: l.seq });
        }
        prev = h;
    }
    Ok(prev)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn chain_ok_and_tamper_detected() {
        let g = genesis_hash();
        let l1 = ChainedLine::next(&g, 1, b"voto1".to_vec());
        let h1 = line_hash(&g, 1, b"voto1");
        let l2 = ChainedLine::next(&h1, 2, b"voto2".to_vec());
        assert!(verify_chain(&[l1.clone(), l2]).is_ok());

        let mut bad = l1;
        bad.payload = b"votoX".to_vec();
        assert!(verify_chain(&[bad]).is_err());
    }

    #[test]
    fn detects_removal() {
        let g = genesis_hash();
        let l1 = ChainedLine::next(&g, 1, b"a".to_vec());
        let h1 = line_hash(&g, 1, b"a");
        let l2 = ChainedLine::next(&h1, 2, b"b".to_vec());
        assert!(verify_chain(std::slice::from_ref(&l2)).is_err());
        let _ = l1;
    }
}
