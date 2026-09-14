# FileServer_Stealer

Windows 가상환경에서 네트워크 공유 드라이브의 파일 탐색 및 압축 과정을 학습하기 위한 교육용 실습 자료입니다.

## 구성 파일

### `FileServerStealer.py`

Python 소스 코드입니다. 실행하면 다음 작업을 수행합니다.

1. Windows에 연결된 드라이브 중 네트워크 드라이브를 확인합니다.
2. 네트워크 드라이브가 정확히 하나일 때만 해당 드라이브를 실습 대상으로 선택합니다.
3. 다음 확장자를 가진 파일을 하위 폴더까지 검색합니다.

	- `.xlsx`, `.xlsm`, `.xls`
	- `.csv`
	- `.hwp`, `.hwpx`
	- `.pdf`, `.ppt`

4. 검색된 파일을 원래 드라이브 기준의 상대 경로로 ZIP 파일에 저장합니다.
5. 결과 ZIP을 `C:\Windows\Temp\Kisec`에 생성합니다.

네트워크 드라이브가 없거나 여러 개이면 자동 수집을 중단하도록 되어 있습니다. 네트워크 드라이브 설정은 **네트워크 공유 드라이브 설정** 단계에서 확인할 수 있습니다.

### `FileServerStealer.exe`

`FileServerStealer.py`를 PyInstaller로 패키징한 Windows 콘솔 실행 파일입니다. 소스 코드와 기본 동작은 동일합니다.

### `Backup/FileServerStealer.exe`

백업 폴더에 보관된 실행 파일입니다. 실습 전에 본 폴더의 실행 파일과 해시 또는 빌드 시점을 비교해 보세요.

### `FileServerStealer.spec`

PyInstaller 빌드 설정 파일입니다. 콘솔 창을 사용하는 실행 파일 이름과 패키징 옵션을 확인할 수 있습니다.

## 실행 결과

성공하면 다음 형식의 파일이 생성됩니다.

```text
C:\Windows\Temp\Kisec\Kisec_Documents_YYYYMMDD_HHMMSS.zip
```

파일 접근 권한이 없거나 읽을 수 없는 항목은 건너뛰고 콘솔에 기록합니다. 테스트 파일이 없으면 ZIP을 만들지 않고 종료합니다.

## 실습 환경

- Windows 가상머신에서 실행합니다.
- 프로젝트별 Python 가상환경을 사용합니다.
- 별도로 허가된 테스트용 네트워크 공유 폴더를 연결합니다.
- 공유 폴더에는 개인정보나 실제 업무 문서 대신 가짜 확장자별 테스트 파일만 둡니다.
- 소스 파일과 EXE를 동시에 실행하지 않습니다.

Python 소스를 분석할 때는 표준 라이브러리만 사용하므로 별도 패키지 설치가 필요하지 않습니다.

## 네트워크 공유 드라이브 설정

FileServerStealer.py는 Windows에 연결된 네트워크 드라이브를 탐색한 뒤, 네트워크 드라이브가 정확히 하나일 때 해당 드라이브를 실습 대상으로 사용합니다.

**File Server 설정**
<br/>
File Server에서 다음 작업을 수행합니다.

1. C:\CompanyData 폴더를 생성한 후 실습용 테스트 데이터를 저장합니다.
2. CompanyData 폴더를 우클릭한 뒤 
Properties → Sharing → Advanced Sharing… 으로 이동합니다. 
3. Share this folder를 체크하고 Share name을 다음과 같이 설정합니다. 
- Share name : Company Data
4. Permissions에서 접근권한을 Read로 부여합니다.
5. 폴더의 Properties → Security → Edit에서 실습용 계정인 employee01를 선택하고 NTFS 권한을 Full control Allow으로 설정합니다.
6. CMD를 실행한 뒤 다음 명령으로 공유 폴더가 정상적으로 등록되었는지 확인합니다.
`net share'
출력 목록에 **CompanyData**가 표시되면 공유 설정이 완료된 것입니다.

**Victim PC에서 공유 폴더를 확인**

1. win + R을 누른 뒤 다음 경로를 입력합니다.
`\\FILE-SRV\CompanyData` 
호스트 이름으로 접속되지 않는 경우 File Server의 IP 주소를 사용합니다.
`\\192.168.60.20\CompanyData`
2. 공유 폴더가 열리고 내부의 테스트 파일이 정상적으로 표시되면 연결이 완료된 것입니다.

## 관찰할 내용

- `GetDriveTypeW`를 이용한 네트워크 드라이브 판별
- `Path.rglob()`을 이용한 하위 폴더 검색
- 확장자 대소문자 정규화와 대상 파일 필터링
- ZIP 내부 상대 경로 보존
- `PermissionError`와 `OSError` 처리
- 타임스탬프를 사용한 결과 파일 이름 생성

## 안전 주의

이 코드는 파일을 탐색하고 복사·압축하는 동작을 포함하므로 실제 PC, 운영 네트워크, 타인의 공유 폴더에서 실행하면 안 됩니다. 반드시 허가받은 격리 VM에서만 실행하고, 실습 후 생성된 ZIP을 삭제하거나 VM 스냅샷을 복원하세요.