# yaft-dalmoori-8x8

[yaft](https://github.com/uobikiemukot/yaft) 프레임버퍼 터미널 + 한글 입력 패치 + **달무리(dalmoori) 8x8 도트폰트** 빌드.

X/Wayland 없이 TTY 프레임버퍼에서 한글을 쓰기 위한 구성입니다.
ThinkPad X61 (1024x768) 기준으로 **256칸 × 96줄** 터미널이 됩니다.

![screenshot](docs/yaft-8x8.png)

cmatrix도 256칸으로:

![cmatrix](docs/larp-cmatrix.png)

## 구성

| 항목 | 내용 |
|---|---|
| 베이스 | yaft (MIT, haru) |
| 한글 입력 | libhangul 기반 패치 (ESC 로 영문 전환, 한/영 키 = `0x80` 센티널) |
| 폰트 | [달무리](https://github.com/RanolP/dalmoori-font) (Apache-2.0) TTF → BDF 변환 |
| 셀 크기 | 4×8 px (ASCII 4px 폭 / 한글 8px 폭 = 2셀) |
| 해상도 | 1024×768 → 256 cols × 96 lines |

원본 한글 패치는 [이 가이드](http://ith.kr/)에서 배포된 것을 기반으로 했고,
16×16 한글도깨비 폰트 대신 8×8 달무리 폰트를 쓰도록 수정했습니다.

## 빌드

의존성:

```bash
sudo pacman -S --needed base-devel libhangul
# 폰트를 다시 만들 경우에만
yay -S otf2bdf
```

빌드:

```bash
# mkfont_bdf 는 wcwidth() 로 글리프 폭을 판정하므로 UTF-8 로케일이 필수
export LANG=ko_KR.UTF-8
make clean
make
sudo make install
```

`glyph.h` 는 `fonts/dalmoori/dalmoori8.bdf` 로부터 자동 생성됩니다.

## 폰트 다시 만들기

```bash
otf2bdf -p 8 -r 72 -o fonts/dalmoori/dalmoori8.bdf /usr/share/fonts/dalmoori/dalmoori.ttf
make clean && make
```

`otf2bdf` 는 8px 픽셀 크기로 비트맵을 뽑습니다.
달무리는 ASCII 4px / 한글 8px 폭으로 설계되어 있어, 그대로 셀 폭 4px 터미널이 됩니다.

## 한/영 키 설정 (콘솔 키맵)

yaft 는 한/영 키를 `0x80` 바이트로 인식합니다.

```bash
sudo mkdir -p /etc/kbd
sudo dumpkeys > /etc/kbd/yaft.kmap
cat <<'EOF' | sudo tee -a /etc/kbd/yaft.kmap
string F60 = "\200"
keycode 100 = F60
keycode 130 = F60
EOF

sudo mkdir -p /usr/share/kbd/keymaps/custom
sudo sh -c 'gzip -c /etc/kbd/yaft.kmap > /usr/share/kbd/keymaps/custom/yaft.map.gz'
sudo loadkeys custom/yaft

# 부팅 시 자동 적용
sudo sed -i 's/^KEYMAP=.*/KEYMAP=custom\/yaft/' /etc/vconsole.conf
```

키코드는 기종마다 다를 수 있습니다. TTY 에서 `showkey` 로 확인해
`keycode N = F60` 을 맞춰 주세요.

## 실행

TTY 로그인 후:

```bash
yaft          # 기본 셸 (zsh) 실행
yaft <명령>    # 특정 명령 실행
```

- 한/영 키: 한글 모드 토글
- ESC: 영문 모드로 전환 (vim 등에서 편의)
- `YAFT_FB_W`, `YAFT_FB_H` 환경변수로 해상도 강제 가능

## 알려진 사항

- 빌드와 실행 모두 UTF-8 로케일이어야 합니다 (`LANG=ko_KR.UTF-8`).
- 4px 폭 ASCII 는 작게 보입니다. 8px 폭이 필요하면 8px BDF 를 만들어
  makefile 의 `mkfont_bdf` 인자만 바꾸면 됩니다.
- 콘솔 키맵을 바꾸므로, yaft 밖의 TTY 에서 한/영 키는 `0x80` 을 출력합니다.

## 라이선스

- yaft: MIT (LICENSE)
- 달무리 폰트: Apache-2.0 (`fonts/dalmoori/LICENSE`)
- 한글 입력 패치: 원 배포자 가이드 기준
