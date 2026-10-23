# 한/영 키 키맵 설정

yaft 의 한글 패치는 한/영 키를 **0x80 바이트**로 구분합니다.
콘솔 키맵에서 사용하지 않는 펑션키(F60 등)에 `"\200"` 문자열을 붙이고,
한/영 키코드를 그 펑션키로 바꿉니다.

```bash
sudo mkdir -p /etc/kbd
sudo dumpkeys > /etc/kbd/yaft.kmap

cat <<'EOF' | sudo tee -a /etc/kbd/yaft.kmap

# --- yaft hangul key (HANGUL_KEY 0x80) ---
string F60 = "\200"
keycode 100 = F60
keycode 130 = F60
EOF

sudo mkdir -p /usr/share/kbd/keymaps/custom
sudo sh -c 'gzip -c /etc/kbd/yaft.kmap > /usr/share/kbd/keymaps/custom/yaft.map.gz'
sudo loadkeys custom/yaft
```

부팅 시 자동 적용:

```bash
sudo sed -i 's/^KEYMAP=.*/KEYMAP=custom\/yaft/' /etc/vconsole.conf
```

## 키코드 찾기

TTY 에서 root 로:

```bash
showkey
```

한/영 키를 눌러 나오는 키코드를 확인한 뒤 `keycode N = F60` 으로 맞춥니다.
(일반적으로 100 = 오른쪽 Alt/한영, 130 = 별도 한/영 키)
