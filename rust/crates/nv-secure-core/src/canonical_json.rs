// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use serde_json::Value;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum CanonicalError {
    #[error("valor não canônico: {0}")]
    NonCanonical(String),
    #[error("serde: {0}")]
    Serde(String),
}

pub fn to_canonical_bytes(value: &Value) -> Result<Vec<u8>, CanonicalError> {
    let mut out = Vec::with_capacity(256);
    write_canonical(value, &mut out)?;
    Ok(out)
}

fn write_canonical(v: &Value, out: &mut Vec<u8>) -> Result<(), CanonicalError> {
    match v {
        Value::Null => out.extend_from_slice(b"null"),
        Value::Bool(true) => out.extend_from_slice(b"true"),
        Value::Bool(false) => out.extend_from_slice(b"false"),
        Value::Number(n) => {
            if let Some(i) = n.as_i64() {
                out.extend_from_slice(itoa::Buffer::new().format(i).as_bytes());
            } else if let Some(u) = n.as_u64() {
                out.extend_from_slice(itoa::Buffer::new().format(u).as_bytes());
            } else if let Some(f) = n.as_f64() {
                if !f.is_finite() {
                    return Err(CanonicalError::NonCanonical("NaN/Inf".into()));
                }
                let mut buf = ryu::Buffer::new();
                out.extend_from_slice(buf.format(f).as_bytes());
            } else {
                return Err(CanonicalError::NonCanonical("number".into()));
            }
        }
        Value::String(s) => {
            let enc = serde_json::to_string(s).map_err(|e| CanonicalError::Serde(e.to_string()))?;
            out.extend_from_slice(enc.as_bytes());
        }
        Value::Array(items) => {
            out.push(b'[');
            for (i, item) in items.iter().enumerate() {
                if i > 0 {
                    out.push(b',');
                }
                write_canonical(item, out)?;
            }
            out.push(b']');
        }
        Value::Object(map) => {
            let mut keys: Vec<&String> = map.keys().collect();
            keys.sort();
            out.push(b'{');
            for (i, k) in keys.iter().enumerate() {
                if i > 0 {
                    out.push(b',');
                }
                let enc_k =
                    serde_json::to_string(k).map_err(|e| CanonicalError::Serde(e.to_string()))?;
                out.extend_from_slice(enc_k.as_bytes());
                out.push(b':');
                write_canonical(&map[*k], out)?;
            }
            out.push(b'}');
        }
    }
    Ok(())
}

pub fn parse_strict(s: &str) -> Result<Value, CanonicalError> {
    if s.len() > 65_536 {
        return Err(CanonicalError::NonCanonical("body > 64KiB".into()));
    }
    serde_json::from_str(s).map_err(|e| CanonicalError::Serde(e.to_string()))
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn orders_keys() {
        let v = json!({"b":1,"a":2});
        assert_eq!(to_canonical_bytes(&v).unwrap(), b"{\"a\":2,\"b\":1}");
    }

    #[test]
    fn rejects_nan() {
        assert!(serde_json::Number::from_f64(f64::NAN).is_none());
        assert!(serde_json::Number::from_f64(f64::INFINITY).is_none());
        let v = serde_json::json!({"x": 1.5});
        assert_eq!(to_canonical_bytes(&v).unwrap(), b"{\"x\":1.5}");
    }

    #[test]
    fn rejects_oversize() {
        let big = "x".repeat(65_537);
        assert!(parse_strict(&big).is_err());
    }
}
