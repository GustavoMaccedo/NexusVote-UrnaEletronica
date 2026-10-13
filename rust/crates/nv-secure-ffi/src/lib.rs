// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

use nv_secure_core::{crypto, sealed_journal};
use std::slice;
use std::sync::Mutex;
use zeroize::Zeroize;

#[allow(unsafe_code)]
mod ffi_inner {}

#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NvErr {
    Ok = 0,
    BadArg = -1,
    BadSig = -2,
    BadChain = -3,
    Rng = -4,
    TooSmall = -5,
    Panic = -99,
}

static SESSION: Mutex<Option<SessionState>> = Mutex::new(None);

struct SessionState {
    writer: sealed_journal::JournalWriter,
}

impl Drop for SessionState {
    fn drop(&mut self) {}
}

fn catch_panic<F: FnOnce() -> i32 + std::panic::UnwindSafe>(f: F) -> i32 {
    match std::panic::catch_unwind(f) {
        Ok(v) => v,
        Err(_) => NvErr::Panic as i32,
    }
}

#[no_mangle]
pub extern "C" fn nv_init_sealed_session() -> i32 {
    catch_panic(|| match crypto::Keypair::generate() {
        Ok(kp) => {
            let st = SessionState {
                writer: sealed_journal::JournalWriter::new(kp),
            };
            *SESSION.lock().unwrap() = Some(st);
            NvErr::Ok as i32
        }
        Err(_) => NvErr::Rng as i32,
    })
}

#[no_mangle]
pub unsafe extern "C" fn nv_pubkey(out: *mut u8, out_len: usize) -> i32 {
    catch_panic(|| {
        if out.is_null() || out_len < 65 {
            return NvErr::TooSmall as i32;
        }
        let guard = SESSION.lock().unwrap();
        let Some(st) = guard.as_ref() else {
            return NvErr::BadArg as i32;
        };
        let hex = st.writer.pubkey_hex().as_bytes();
        #[allow(unsafe_code)]
        unsafe {
            std::ptr::copy_nonoverlapping(hex.as_ptr(), out, hex.len());
            *out.add(hex.len()) = 0;
        }
        hex.len() as i32
    })
}

#[no_mangle]
pub unsafe extern "C" fn nv_seal_vote(
    data: *const u8,
    data_len: usize,
    out: *mut u8,
    out_len: usize,
) -> i32 {
    catch_panic(|| {
        if data.is_null() || out.is_null() || data_len == 0 || data_len > 65_536 {
            return NvErr::BadArg as i32;
        }
        #[allow(unsafe_code)]
        let (din, dout) = unsafe {
            (
                slice::from_raw_parts(data, data_len),
                slice::from_raw_parts_mut(out, out_len),
            )
        };
        let value: serde_json::Value = match serde_json::from_slice(din) {
            Ok(v) => v,
            Err(_) => return NvErr::BadArg as i32,
        };
        let mut guard = SESSION.lock().unwrap();
        let Some(st) = guard.as_mut() else {
            return NvErr::BadArg as i32;
        };
        let line = match st.writer.seal(value) {
            Ok(l) => l,
            Err(_) => return NvErr::BadChain as i32,
        };
        let s = match sealed_journal::line_to_jsonl(&line) {
            Ok(s) => s,
            Err(_) => return NvErr::BadArg as i32,
        };
        if s.len() + 1 > dout.len() {
            return NvErr::TooSmall as i32;
        }
        dout[..s.len()].copy_from_slice(s.as_bytes());
        s.len() as i32
    })
}

#[no_mangle]
pub unsafe extern "C" fn nv_verify(
    pubkey: *const u8,
    sig: *const u8,
    msg: *const u8,
    msg_len: usize,
) -> i32 {
    catch_panic(|| {
        if pubkey.is_null() || sig.is_null() || msg.is_null() {
            return NvErr::BadArg as i32;
        }
        #[allow(unsafe_code)]
        let (pk, sg, mg) = unsafe {
            (
                slice::from_raw_parts(pubkey, 32),
                slice::from_raw_parts(sig, 64),
                slice::from_raw_parts(msg, msg_len),
            )
        };
        match crypto::verify_ed25519(pk, mg, sg) {
            Ok(()) => NvErr::Ok as i32,
            Err(_) => NvErr::BadSig as i32,
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn nv_sha256(
    msg: *const u8,
    msg_len: usize,
    out: *mut u8,
    out_len: usize,
) -> i32 {
    catch_panic(|| {
        if msg.is_null() || out.is_null() || out_len < 32 {
            return NvErr::BadArg as i32;
        }
        #[allow(unsafe_code)]
        let (mg, og) = unsafe {
            (
                slice::from_raw_parts(msg, msg_len),
                slice::from_raw_parts_mut(out, out_len),
            )
        };
        let h = crypto::sha256(mg);
        og[..32].copy_from_slice(&h);
        let mut tmp = h;
        tmp.zeroize();
        NvErr::Ok as i32
    })
}

#[no_mangle]
pub unsafe extern "C" fn nv_hash_pin(
    pin: *const u8,
    pin_len: usize,
    out: *mut u8,
    out_len: usize,
) -> i32 {
    catch_panic(|| {
        if pin.is_null() || out.is_null() || pin_len == 0 || pin_len > 128 || out_len < 64 {
            return NvErr::BadArg as i32;
        }
        #[allow(unsafe_code)]
        let pin_bytes = unsafe { slice::from_raw_parts(pin, pin_len) };
        let pin_str = match std::str::from_utf8(pin_bytes) {
            Ok(s) => s.to_string(),
            Err(_) => return NvErr::BadArg as i32,
        };
        let p = match nv_secure_core::pin_auth::Pin::new(pin_str) {
            Ok(p) => p,
            Err(_) => return NvErr::BadArg as i32,
        };
        let phc = match nv_secure_core::pin_auth::hash_pin(&p) {
            Ok(h) => h,
            Err(_) => return NvErr::Rng as i32,
        };
        #[allow(unsafe_code)]
        let og = unsafe { slice::from_raw_parts_mut(out, out_len) };
        if phc.len() + 1 > og.len() {
            return NvErr::TooSmall as i32;
        }
        og[..phc.len()].copy_from_slice(phc.as_bytes());
        og[phc.len()] = 0;
        phc.len() as i32
    })
}

#[no_mangle]
pub unsafe extern "C" fn nv_verify_pin(
    pin: *const u8,
    pin_len: usize,
    phc: *const u8,
    phc_len: usize,
) -> i32 {
    catch_panic(|| {
        if pin.is_null()
            || phc.is_null()
            || pin_len == 0
            || pin_len > 128
            || phc_len == 0
            || phc_len > 512
        {
            return NvErr::BadArg as i32;
        }
        #[allow(unsafe_code)]
        let (pin_bytes, phc_bytes) = unsafe {
            (
                slice::from_raw_parts(pin, pin_len),
                slice::from_raw_parts(phc, phc_len),
            )
        };
        let (Ok(pin_str), Ok(phc_str)) = (
            std::str::from_utf8(pin_bytes),
            std::str::from_utf8(phc_bytes),
        ) else {
            return NvErr::BadArg as i32;
        };
        let Ok(p) = nv_secure_core::pin_auth::Pin::new(pin_str.to_string()) else {
            return NvErr::BadSig as i32;
        };
        match nv_secure_core::pin_auth::verify_pin(&p, phc_str) {
            Ok(()) => NvErr::Ok as i32,
            Err(_) => NvErr::BadSig as i32,
        }
    })
}
