# Network Stealer 

Windows API를 이용해 네트워크 인터페이스 정보를 로컬 보고서로 만드는 과정을 분석하기 위한 교육용 자료입니다. 외부 전송 기능은 포함하지 않지만, 보고서에는 시스템 식별 정보와 네트워크 주소가 포함될 수 있습니다.

## 파일 설명

- `kisec_winapi_report.py`: `ctypes`로 `GetComputerNameExW`, `GetAdaptersAddresses`, `GetBestInterface`를 호출하여 컴퓨터 이름, 어댑터 상태, MAC 주소, IP/DNS/게이트웨이와 선택된 인터페이스 정보를 수집합니다. 보고서는 `C:\Windows\Temp\Kisec\network_info.txt`에 UTF-8 BOM 형식으로 저장됩니다.
- `KisecWinApiReport.exe`: 위 Python 파일을 Windows 실행 파일로 패키징한 결과물입니다.

## 실행 전제

- Windows 가상머신이 필요하며, Python 소스 확인 시 프로젝트 전용 가상환경을 사용하세요.
- 별도 Python 패키지 없이 Windows 기본 DLL과 표준 라이브러리만 사용합니다.
- 실행 전 VM의 네트워크 설정과 보고서 생성 위치를 확인하고, 보고서가 실습 환경 밖으로 이동하지 않도록 하세요.

## 관찰 포인트

Win32 API의 `ctypes` 선언, 가변 길이 버퍼 재시도, 연결 리스트 순회, IPv4/IPv6 주소 변환, API 오류 코드 기록 방식을 확인할 수 있습니다.

## 안전 주의

보고서에는 MAC 주소와 IP 주소가 기록될 수 있으므로 결과 파일을 공개 저장소나 외부 시스템에 업로드하지 마세요. 허가받은 VM에서만 실행하고, 실습 후 보고서를 삭제하거나 스냅샷을 복원하세요.