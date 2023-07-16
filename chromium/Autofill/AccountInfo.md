# Chromium 的账号信息

Chrome 的账号信息存放在 `$ChromeProfilePath/Login Data` 之中，由 login_database.cc 负责读写，（基于安全考虑，力求就算 Login Data 文件被别人获取 也无法解读里面的密码）其中利用了系统内置的加密功能，
譬如 Mac 的 KeyChain,  Linux 的 kWallet, Windows 的 DPAPI，进行加密。

> Profile Path 可以去 chrome://version 搜索找到

> 只有密码是加密的，账号名等信息是明文的。

账号存放的 table 信息:
```sql
CREATE TABLE logins (
  origin_url VARCHAR NOT NULL, action_url VARCHAR,
  username_element VARCHAR, username_value VARCHAR,   # <==== Username
  password_element VARCHAR, password_value BLOB,      # <==== Password
  submit_element VARCHAR, signon_realm VARCHAR NOT NULL, date_created INTEGER NOT NULL,
  blacklisted_by_user INTEGER NOT NULL, scheme INTEGER NOT NULL,
  password_type INTEGER, times_used INTEGER, form_data BLOB, display_name VARCHAR, icon_url VARCHAR, federation_url VARCHAR,
  skip_zero_click INTEGER, generation_upload_status INTEGER, possible_username_pairs BLOB,
  id INTEGER PRIMARY KEY AUTOINCREMENT, date_last_used INTEGER NOT NULL DEFAULT 0, moving_blocked_for BLOB, date_password_modified INTEGER NOT NULL DEFAULT 0, UNIQUE (origin_url, username_element, username_value, password_element, signon_realm)
);
```

## 加解密

加解密利用了系统提供的加密功能，这块代码在 `components/os_crypt` 中，根据不同平台有：

| Platform | File | KeyStore |
| :--: | -- | :--: |
| - | os_crypt.h | - |
| Android | os_crypt_posix.cc | - |
| Apple | os_crypt_mac.mm | KeyChain |
| Linux | os_crypt_linux.cc | kWallet |
| Windows | os_crypt_win.cc | DPAPI |

以 Linux 为例，在 login_database.cc 调用 EncryptString 时，os_crypt_linux：
* 从 KeyStorage KWallet 中获取 encryption key. 
* 调用 AES128 加密算法, 进行加密

上述代码参考 os_crypt_linux.cc EncryptString, key_storage_linux.cc CreateService

所以攻击破解的压力，给到系统存储密钥的地方 OSCrypt KeyStore.

## Android 的拉跨

破解密码的关键在密钥，而 Android 的密钥确是固定的：
os_crypt_posix.cc GetEncryptionKey

```c++
crypto::SymmetricKey* GetEncryptionKey() {
  // We currently "obfuscate" by encrypting and decrypting with hard-coded
  // password.  We need to improve this password situation by moving a secure
  // password into a system-level key store.
  // http://crbug.com/25404 and http://crbug.com/49115
  const std::string password = "peanuts";
  const std::string salt(kSalt);

  // Create an encryption key from our password and salt.
  std::unique_ptr<crypto::SymmetricKey> encryption_key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, password, salt, kEncryptionIterations,
          kDerivedKeySizeInBits));
  DCHECK(encryption_key.get());

  return encryption_key.release();
}
```
这块代码最早可以追述到 2010 年。。。

不过各位也不用担心，因为 Login Data 存放在 `/data/data/org.chrome.chromium/app_chrome/Default` 而 Android 系统是不会让其他 app 随意访问到这个地址的。root 除外（逃

# 后记

Login 信息是非常敏感的，基本上其他平台都会跟该系统的加密功能结合，把压力给到系统加密功能，Android 确实让人直摇头，但是由于其 Login Data 文件本身很难获取到，所以也算是最后的欣慰把。。。
