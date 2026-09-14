# LAB 서버 환경 구성 가이드

ClickFix 기반 침해사고 시뮬레이션 LAB을 동일하게 재현하기 위한 VM 구성 절차입니다. 아래 순서대로 진행하면 동일한 네트워크·서버 환경을 구축할 수 있습니다.

## 0. 전체 네트워크 구조

```
                       VMware Workstation

      VMnet2 - 외부/공격 시나리오망 (Host-only)
                192.168.50.0/24
                       |
        ┌──────────────┼──────────────┐
        |               |              |
        ▼               ▼              ▼
   [Attacker]      [ClickFix Web]   [Victim PC]
   Kali Linux       Ubuntu/Nginx     Windows 10
  192.168.50.10    192.168.50.30    NIC1: 192.168.50.101
                                          |
                                          | (Victim만 두 망에 모두 연결)
                                          ▼
      VMnet3 - 내부 업무망 (Host-only)
                192.168.60.0/24
                       |
        ┌──────────────┼──────────────┐
        |               |              |
        ▼               ▼              ▼
     [AD/DC]       [File Server]   Victim PC
  Windows Server   Windows Server   NIC2: 192.168.60.101
  192.168.60.5      192.168.60.20
```

Victim PC는 외부망(VMnet2)과 내부망(VMnet3)에 동시에 연결된 유일한 VM으로, ClickFix를 통해 유입된 위협이 내부 업무망으로 이동하는 경로 역할을 합니다.

## 1. 최종 IP 주소표

| VM | 역할 | NIC | VMware Network | IP | Gateway | DNS |
| --- | --- | --- | --- | --- | --- | --- |
| VM-01 | Kali Attacker | NIC1 | VMnet2 | `192.168.50.10` | 없음 | 없음 |
| VM-02 | ClickFix Web | NIC1 | VMnet2 | `192.168.50.30` | 없음 | 없음 |
| VM-04 | Victim PC | NIC1 | VMnet2 | `192.168.50.101` | 없음 | 없음 |
| VM-04 | Victim PC | NIC2 | VMnet3 | `192.168.60.101` | 없음 | `192.168.60.5` |
| VM-03 | AD/DC | NIC1 | VMnet3 | `192.168.60.5` | 없음 | `192.168.60.5`(자기 자신) |
| VM-05 | File Server | NIC1 | VMnet3 | `192.168.60.20` | 없음 | `192.168.60.5` |

> Victim PC NIC1(외부망)에는 DNS 서버가 없으므로 DNS를 설정하지 않습니다. `corp.local` 이름 해석이 필요한 내부망 통신은 NIC2에 설정된 DNS(`192.168.60.5`)를 통해 이루어집니다.

공통 실습 계정 암호: `kisec123!@#` (초기 설정) → 최초 로그인 시 `kisec123!@#$`로 변경

---

## 2. VMware 가상 네트워크 설정

VMware Workstation을 사용해 두 개의 Host-only 네트워크를 생성합니다. 개념적으로는 서로 완전히 분리된 스위치 두 개를 만드는 것과 같으며, 기본적으로 VMnet2와 VMnet3은 서로 직접 통신할 수 없습니다.

1. **VMware Workstation → Edit → Virtual Network Editor → Change Settings → Add Network**
2. 아래 두 네트워크를 각각 추가합니다.

| 네트워크 | 타입 | Subnet IP | Subnet Mask | DHCP |
| --- | --- | --- | --- | --- |
| VMnet2 | Host-only | `192.168.50.0` | `255.255.255.0` | OFF |
| VMnet3 | Host-only | `192.168.60.0` | `255.255.255.0` | OFF |

3. 각 VM의 **Settings → Network Adapter**에서 아래와 같이 연결합니다.

| VM | 연결 네트워크 |
| --- | --- |
| Kali, ClickFix Web, Victim PC (NIC1) | VMnet2 (외부/공격 시나리오망) |
| AD/DC, File Server, Victim PC (NIC2) | VMnet3 (내부 업무망) |

AD/DC와 File Server는 Network Adapter를 하나만 사용하며, 반드시 **VMnet3에만** 연결합니다. Victim PC는 Network Adapter를 두 개(VMnet2 + VMnet3) 추가해 두 망 모두에 연결합니다.

---

## 3. VM-01 Attacker (Kali Linux) — `192.168.50.10`

1. [Kali Linux VM 이미지](https://www.kali.org/get-kali/#kali-virtual-machines) 다운로드 후 VMware에 등록
2. 네트워크 인터페이스 설정 파일 수정

   ```bash
   cd /etc/network
   sudo vi interfaces
   ```

3. 아래 내용 입력

   ```
   auto eth0
   iface eth0 inet static
   address 192.168.50.10
   netmask 255.255.255.0
   broadcast 192.168.50.255
   ```

4. 네트워크 재시작 후 설정 확인

   ```bash
   sudo systemctl restart networking
   ip route
   ip addr
   ```

   `ip route` 결과에 아래와 같이 표시되면 정상입니다.

   ```
   192.168.50.0/24 dev eth0 ... src 192.168.50.10
   ```

---

## 4. VM-02 ClickFix Web (Ubuntu Server) — `192.168.50.30`

1. [Ubuntu Server 이미지](https://ubuntu.com/download/server) 다운로드 후 설치
2. Netplan 설정 파일 수정

   ```bash
   sudo vi /etc/netplan/00-installer-config.yaml
   ```

3. 아래 내용 입력

   ```yaml
   network:
     version: 2
     ethernets:
       ens33:
         addresses:
           - 192.168.50.30/24
   ```

4. 설정 적용 및 확인

   ```bash
   sudo netplan apply
   ip addr
   ip route
   ping -c 4 192.168.50.10
   ```

   Kali와 Ubuntu는 같은 VMnet2에 속하므로 서로 정상적으로 ping이 되어야 합니다.

---

## 5. VM-03 AD/DC (Windows Server 2022) — `192.168.60.5`

### 5-1. OS 설치

1. [Windows Server 2022 평가판](https://www.microsoft.com/ko-kr/evalcenter/download-windows-server-2022) 다운로드 후 설치
2. 설치 중 관리자 암호를 `kisec123!@#`로 설정

### 5-2. 네트워크 설정

**Win + R → `ncpa.cpl`** → Ethernet 속성 → **Internet Protocol Version 4 (TCP/IPv4)** → 속성

```
IP      : 192.168.60.5
Subnet  : 255.255.255.0
Gateway : 없음
DNS     : 192.168.60.5   (자기 자신)
```

> AD/DC는 File Server·Victim PC와의 도메인 통신을 위해 반드시 내부 업무망(VMnet3, `192.168.60.0/24`)에 위치해야 합니다. 외부망(`192.168.50.0/24`) IP로 설정하면 도메인 조인 및 이름 해석 오류가 발생하므로(7. 트러블슈팅 참고), 처음부터 `192.168.60.5`로 설정합니다.

설정 후 CMD에서 확인합니다.

```
ipconfig /all
```

다음 항목이 정상인지 확인합니다.

```
IPv4 Address        : 192.168.60.5
Subnet Mask         : 255.255.255.0
Default Gateway     :
DNS Servers         : 192.168.60.5  (::1이 함께 표시될 수 있음, IPv6 loopback이므로 정상)
Primary DNS Suffix  : corp.local
IP Routing Enabled  : No
```

### 5-3. Active Directory Domain Services 설치

1. **Server Manager → Add Roles and Features → Role-based or feature-based installation** → 현재 서버 선택
2. **Active Directory Domain Services** 체크 → 추가 기능 설치 창이 뜨면 **Add Features** 클릭 → Next 계속 → **Install**
3. 설치 완료 후 Server Manager 우측 상단 경고 깃발(⚑) 클릭 → **Promote this server to a domain controller**
4. **Add a new forest** 선택 → Root domain name: `corp.local`
5. DSRM(디렉터리 서비스 복원 모드) 암호를 `kisec123!@#`로 설정 후 계속 Next → 설치 진행
6. 설치 완료 후 재부팅 → 로그인 화면에 `CORP\Administrator`로 표시되면 도메인 컨트롤러 승격 성공

### 5-4. 실습용 도메인 계정 생성

1. **Server Manager → Tools → Active Directory Users and Computers**
2. `corp.local → Users` 우클릭 → **New → User**
3. 아래 정보로 계정 생성

   ```
   User logon name : employee01
   Password         : kisec123!@#
   ```

---

## 6. VM-04 Victim PC (Windows 10) — `192.168.50.101` / `192.168.60.101`

로컬 계정: `DESKTOP-IS00QJN\kisec` / `kisec123`

### 6-1. 네트워크 설정 (이중 NIC)

VMware에서 **Add → Network Adapter**로 두 번째 어댑터를 추가하고 VMnet3에 연결합니다. Windows에서는 **Win + R → `ncpa.cpl`**에서 NIC별로 Internet Protocol Version 4 (TCP/IPv4) 속성을 설정합니다.

| NIC | 연결망 | IP | Subnet | Gateway | DNS |
| --- | --- | --- | --- | --- | --- |
| Ethernet0 (NIC1) | VMnet2 (외부) | `192.168.50.101` | `255.255.255.0` | 없음 | 없음 |
| Ethernet1 (NIC2) | VMnet3 (내부) | `192.168.60.101` | `255.255.255.0` | 없음 | `192.168.60.5` |

> 두 개의 NIC를 구성하는 것이지, Victim PC가 라우터 역할을 하는 것은 아닙니다. 각 NIC는 각자의 네트워크 대역에서만 통신합니다.

### 6-2. 도메인 조인

1. **Win + R → `sysdm.cpl`**
2. **Computer Name → Change…** → **Domain**: `corp.local` 입력
3. 인증 요청 시 `Administrator` / `kisec123!@#` 입력 후 재부팅
4. 최초 로그인 시 비밀번호 변경

   ```
   기존 비밀번호 : kisec123!@#
   새 비밀번호   : kisec123!@#$
   ```

---

## 7. VM-05 File Server (Windows Server 2022) — `192.168.60.20`

### 7-1. OS 설치

1. Windows Server 2022 설치 (VM-03과 동일한 설치 이미지 사용)
2. 설치 중 관리자 암호를 `kisec123!@#`로 설정

### 7-2. 네트워크 설정

**Win + R → `ncpa.cpl`** → Ethernet 속성 → Internet Protocol Version 4 (TCP/IPv4) → 속성

```
IP      : 192.168.60.20
Subnet  : 255.255.255.0
Gateway : 없음
DNS     : 192.168.60.5
```

### 7-3. 도메인 조인

1. **Win + R → `sysdm.cpl`**
2. **Computer Name → Change…** → **Domain**: `corp.local` 입력
3. 인증 요청 시 `Administrator` / `kisec123!@#` 입력 후 재부팅
4. 최초 로그인 시 비밀번호 변경

   ```
   기존 비밀번호 : kisec123!@#
   새 비밀번호   : kisec123!@#$
   ```

---

## 8. 최종 연결 테스트

전체 VM 구성이 끝나면 아래 순서대로 통신을 확인합니다.

### 8-1. 외부망(VMnet2) 테스트

Kali에서:

```bash
ping -c 4 192.168.50.30
ping -c 4 192.168.50.101
```

Ubuntu(ClickFix Web)에서:

```bash
ping -c 4 192.168.50.10
ping -c 4 192.168.50.101
```

목표: `Kali ↔ Ubuntu ↔ Victim(NIC1)` 모두 정상 통신

### 8-2. 내부망(VMnet3) 테스트

AD/DC에서:

```
ping 192.168.60.20
ping 192.168.60.101
```

File Server에서:

```
ping 192.168.60.5
ping 192.168.60.101
```

Victim PC에서:

```
ping 192.168.60.5
ping 192.168.60.20
nslookup corp.local
nslookup FILE-SRV.corp.local
```

`nslookup corp.local` 결과가 아래와 같이 나오면 성공입니다.

```
Server:  Unknown
Address: 192.168.60.5

Name:    corp.local
Address: 192.168.60.5
```

`nslookup FILE-SRV.corp.local` 결과도 아래와 같이 나와야 합니다.

```
Server:  Unknown
Address: 192.168.60.5

Name:    FILE-SRV.corp.local
Address: 192.168.60.20
```

목표: `AD/DC ↔ File Server ↔ Victim(NIC2)` 모두 정상 통신 및 이름 해석 성공

---

## 9. FILE-SRV 공유 폴더(CompanyData) 구성

File Server에 실습용 문서를 담을 SMB 공유 폴더를 구성합니다. 권장 진행 순서는 다음과 같습니다.

```
① FILE-SRV에 공유 폴더 생성
        ↓
② Share 권한 + NTFS 권한을 employee01(및 Administrator)에게 부여
        ↓
③ 두 계정에서 \\FILE-SRV\CompanyData 접근 확인
        ↓
④ 필요 시 Z: 드라이브로 매핑 (선택)
```

Z: 드라이브 매핑은 공유 폴더 권한 설정과 별개의 개념이므로, 실습에서는 매핑 없이 UNC 경로(`\\FILE-SRV\CompanyData`)를 직접 사용하는 것을 권장합니다. 학생 PC 환경 차이로 인한 문제를 줄일 수 있습니다.

### 9-1. 폴더 생성

File Server에서 `C:\CompanyData` 폴더를 생성합니다(이미 존재하면 그대로 사용). 실습용 더미 문서(/Dummy_Data)는 이 폴더 아래에 배치합니다.

### 9-2. SMB 공유(Sharing) 설정

`CompanyData` 우클릭 → **Properties → Sharing → Advanced Sharing**

```
☑ Share this folder
Share name : CompanyData
```

**Permissions** 버튼을 눌러 공유 권한을 설정합니다. 실습 계정(`CORP\employee01`)에게만 읽기 권한을 부여하고, 기본으로 추가되어 있는 `Everyone`은 제거하는 것을 권장합니다.

```
Share Permissions
CORP\employee01     : Read
Everyone             : 제거
```

> 접근 테스트 단계에서만 임시로 `Everyone : Read`를 사용해도 되지만, 최종 구성에서는 실습 계정으로 범위를 좁히는 것이 좋습니다.

### 9-3. NTFS(Security) 권한 설정

`CompanyData → Properties → Security → Edit → Add`

`Enter the object names to select`에 `employee01` 입력 후 **Check Names** 클릭합니다.

- File Server가 `corp.local` 도메인에 정상적으로 가입되어 있어야 AD 계정을 조회할 수 있습니다. 도메인 가입 여부는 CMD에서 아래 명령으로 확인할 수 있습니다.

  ```
  systeminfo | findstr /B /C:"Domain"
  ```

  결과가 `Domain: corp.local`이어야 정상입니다.

- 계정을 찾지 못하면 **Locations…** 버튼을 눌러 조회 범위를 `FILE-SRV`(로컬)가 아닌 `corp.local`(도메인)로 변경한 뒤 다시 `employee01`을 입력하고 **Check Names**를 클릭합니다. 목표는 `FILE-SRV\employee01`이 아니라 `CORP\employee01`(AD 계정)을 추가하는 것입니다.
- 이 과정에서 도메인 자격 증명을 요구하는 창이 뜰 수 있습니다. 이는 File Server가 AD 개체 조회 권한이 있는 계정으로 인증하려는 것으로, `employee01`의 비밀번호가 아니라 **AD/DC 관리자 계정**(`CORP\Administrator` 또는 `Administrator@corp.local`)과 그 비밀번호를 입력하면 됩니다.

계정이 정상적으로 인식되면 목록에서 선택한 뒤 권한을 아래와 같이 읽기 전용으로 설정합니다.

```
Permissions for employee01
Allow
☑ Read & execute
☑ List folder contents
☑ Read
☐ Modify / Write / Full control
```

`Apply → OK`로 저장합니다. 필요하다면 `CORP\Administrator` 계정도 동일한 방식으로 추가할 수 있습니다.

최종 권한 구조는 다음과 같습니다.

```
C:\CompanyData
        │
        ├─ Share Permission
        │      └─ CORP\employee01 : Read
        │
        └─ NTFS(Security) Permission
               └─ CORP\employee01
                    ├─ Read & execute
                    ├─ List folder contents
                    └─ Read
```

Share 권한과 NTFS 권한은 둘 다 통과해야 실제 접근이 허용됩니다. 하나라도 막혀 있으면 접근이 거부됩니다.

### 9-4. 공유 생성 확인

File Server CMD에서:

```
net share
```

목록에 아래 항목이 보이면 SMB 공유가 정상적으로 생성된 것입니다.

```
Share name     Resource
---------------------------------
CompanyData    C:\CompanyData
```

### 9-5. Victim PC에서 접근 확인

`CORP\employee01` 계정으로 로그인한 Victim PC에서 PowerShell로 확인합니다.

```powershell
Test-Path \\FILE-SRV\CompanyData
Get-ChildItem \\FILE-SRV\CompanyData
```

또는 **Win + R** → `\\FILE-SRV\CompanyData` 입력으로 탐색기에서 직접 열어봐도 됩니다. 필요하다면 `CORP\Administrator` 계정으로도 동일하게 접근되는지 확인합니다.

읽기 전용으로 구성했다면 다음과 같이 동작해야 합니다.

```
파일 목록 조회      ✅
파일 열기/복사       ✅
서버에 새 파일 생성   ❌
서버 파일 수정/삭제   ❌
```

---

## 10. 트러블슈팅: `CORP\Administrator` 로그인이 안 될 때

도메인 멤버 서버/PC에서 `CORP\Administrator`로 로그인이 되지 않는다면, AD/DC의 IP·DNS 설정이 잘못되어 있을 가능성이 높습니다. 특히 AD/DC를 다른 네트워크(예: 외부망 `192.168.50.x`)에서 내부망(`192.168.60.x`)으로 옮긴 경우 자주 발생합니다.

### 10-1. AD/DC IP 변경 절차 (네트워크를 옮겨야 하는 경우)

1. Windows Server를 정상적으로 종료합니다. (**Start → Power → Shut down**)
2. VMware에서 해당 VM이 `Powered off` 상태인지 확인 후, **Edit virtual machine settings → Network Adapter**에서 연결 네트워크를 **Custom: VMnet3 (Host-only)**로 변경하고 `Connect at power on`이 체크되어 있는지 확인합니다.
3. VM을 다시 켜고 Windows에서 **Win + R → `ncpa.cpl`** → Ethernet 속성에서 IP를 최종값(`192.168.60.5`)으로 변경합니다.
4. `ipconfig /all`로 변경된 IP·DNS·Primary DNS Suffix(`corp.local`)를 확인합니다.

### 10-2. DNS 레코드 확인 및 정리

IP를 변경해도 DNS 서버(AD 통합 DNS) 안에는 예전 IP를 가리키는 레코드가 남아 있을 수 있습니다.

1. **AD/DC의 DNS 정보 확인**

   ```
   nslookup corp.local
   ```

   응답에 옛 IP(예: `192.168.50.5`)가 나온다면 DNS 레코드가 갱신되지 않은 상태입니다.

2. **DNS Manager에서 레코드 직접 확인**

   `Server Manager → Tools → DNS` → `DNS → (서버명) → Forward Lookup Zones → corp.local`을 펼쳐 오른쪽 레코드 목록을 확인합니다. 아래와 같이 예전 IP를 가리키는 **Host (A)** 레코드가 있는지 찾습니다.

   ```
   Name                  Type        Data
   ------------------------------------------------
   (same as parent...)   Host (A)    192.168.50.5   ← 예전 IP
   <DC 호스트명>          Host (A)    192.168.50.5   ← 예전 IP
   ```

   무작정 전체 삭제하지 말고, **DC를 가리키는 레코드만** 확인합니다. 해당 레코드를 더블클릭(Properties)하여 IP를 새 값(`192.168.60.5`)으로 수정하거나, 수정이 안 되는 경우 기존 레코드를 삭제한 뒤 같은 이름으로 새 Host (A) 레코드를 생성합니다. `(same as parent folder)` 루트 레코드와 DC 호스트명 레코드 **둘 다** 확인이 필요합니다.

3. **DNS 캐시 갱신**

   AD/DC에서:

   ```
   ipconfig /flushdns
   ipconfig /registerdns
   net stop netlogon
   net start netlogon
   ```

4. **SRV 레코드 확인**

   ```
   nslookup -type=SRV _ldap._tcp.dc._msdcs.corp.local 192.168.60.5
   ```

5. **Victim PC / 멤버 서버에서 DNS 캐시 초기화**

   ```
   ipconfig /flushdns
   nltest /dsgetdc:corp.local
   ```

6. **최종 확인**

   ```
   nslookup corp.local
   nslookup <DC 호스트명>.corp.local
   ```

   두 명령 모두 새 IP(`192.168.60.5`)를 정상적으로 반환하면 DC IP 이전과 DNS 정리가 완료된 것입니다. 이후 `CORP\Administrator` 로그인이 정상적으로 가능해집니다.
