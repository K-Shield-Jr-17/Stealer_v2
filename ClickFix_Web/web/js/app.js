const subjects = {
  infra: { title: "IT 인프라 이해하기", number: "SUBJECT 01", vm: 2, file: 1, url: 1, fileName: "K-Shield_Jr_IR_Lab_Resources.zip" },
  system: { title: "시스템 보안", number: "SUBJECT 02", vm: 1, file: 2, url: 0, fileName: "System_Security_Checklist.zip" },
  initial: { title: "침해사고 초동대응", number: "SUBJECT 03", vm: 2, file: 2, url: 1, fileName: "IR_Initial_Response_Kit.zip" },
  forensics: { title: "메모리/디스크 분석", number: "SUBJECT 04", vm: 2, file: 3, url: 0, fileName: "Memory_Disk_Artifacts.zip" },
  case: { title: "Case별 침해사고 분석", number: "SUBJECT 05", vm: 3, file: 2, url: 1, fileName: "ClickFix_Case_Evidence.zip" },
  malware: { title: "악성코드 분석 실무", number: "SUBJECT 06", vm: 2, file: 2, url: 0, fileName: "Safe_Malware_Analysis_Samples.zip" }
};

const subjectTitle = document.getElementById("subjectTitle");
const subjectNumber = subjectTitle.parentElement.querySelector(".eyebrow");
const vmCount = document.getElementById("vmCount");
const fileCount = document.getElementById("fileCount");
const urlCount = document.getElementById("urlCount");
const fileName = document.getElementById("fileName");
const toast = document.getElementById("toast");
let toastTimer;

function showToast(message) {
  toast.textContent = message;
  toast.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { toast.hidden = true; }, 2800);
}

document.querySelectorAll(".subject-button").forEach((button) => {
  button.addEventListener("click", () => {
    const subject = subjects[button.dataset.subject];
    if (!subject) return;
    document.querySelectorAll(".subject-button").forEach((item) => item.classList.remove("is-selected"));
    button.classList.add("is-selected");
    subjectTitle.textContent = subject.title;
    subjectNumber.textContent = subject.number;
    vmCount.textContent = subject.vm;
    fileCount.textContent = subject.file;
    urlCount.textContent = subject.url;
    fileName.textContent = subject.fileName;
    document.getElementById("courseSidebar").classList.remove("is-open");
    document.getElementById("mobileMenuButton").setAttribute("aria-expanded", "false");
  });
});

function setupDropdown(buttonId, panelId) {
  const button = document.getElementById(buttonId);
  const panel = document.getElementById(panelId);
  button.addEventListener("click", (event) => {
    event.stopPropagation();
    const willOpen = panel.hidden;
    document.querySelectorAll(".dropdown").forEach((item) => { item.hidden = true; });
    document.querySelectorAll("[aria-controls]").forEach((item) => {
      if (item !== document.getElementById("mobileMenuButton")) item.setAttribute("aria-expanded", "false");
    });
    panel.hidden = !willOpen;
    button.setAttribute("aria-expanded", String(willOpen));
  });
}

setupDropdown("notificationButton", "notificationPanel");
setupDropdown("profileButton", "profilePanel");

document.addEventListener("click", () => {
  document.querySelectorAll(".dropdown").forEach((item) => { item.hidden = true; });
  document.getElementById("notificationButton").setAttribute("aria-expanded", "false");
  document.getElementById("profileButton").setAttribute("aria-expanded", "false");
});

document.querySelectorAll(".dropdown").forEach((panel) => panel.addEventListener("click", (event) => event.stopPropagation()));

const mobileMenuButton = document.getElementById("mobileMenuButton");
mobileMenuButton.addEventListener("click", () => {
  const sidebar = document.getElementById("courseSidebar");
  const isOpen = sidebar.classList.toggle("is-open");
  mobileMenuButton.setAttribute("aria-expanded", String(isOpen));
});

function openModal(modal) {
  modal.hidden = false;
  document.body.style.overflow = "hidden";
  modal.querySelector("button").focus();
}

function closeModal(modal) {
  modal.hidden = true;
  document.body.style.overflow = "";
}

const tokenModal = document.getElementById("tokenModal");
document.getElementById("tokenButton").addEventListener("click", () => {
  const suffix = String(Date.now()).slice(-6);
  document.getElementById("tokenValue").textContent = `LAB-206-${suffix}`;
  openModal(tokenModal);
});

const scenarioModal = document.getElementById("scenarioModal");
document.getElementById("scenarioButton").addEventListener("click", () => openModal(scenarioModal));

const allowedDemoCommand = 'cmd /k echo "Hello World!"';

async function loadDemoCommand() {
  // Nginx의 /c2/ 경로가 Kali 모의 C2(192.168.50.10:8000)로 프록시된다.
  const response = await fetch("/c2/lab-command.txt", { cache: "no-store" });
  if (!response.ok) throw new Error("Demo command request failed");

  const command = (await response.text()).trim();
  if (command !== allowedDemoCommand) throw new Error("Unexpected demo command");
  return command;
}

async function copyText(text) {
  if (navigator.clipboard && window.isSecureContext) {
    try {
      await navigator.clipboard.writeText(text);
      return;
    } catch {
      // 권한이 거부되면 아래의 호환 복사 방식으로 전환한다.
    }
  }

  const textArea = document.createElement("textarea");
  textArea.value = text;
  textArea.setAttribute("readonly", "");
  textArea.style.position = "fixed";
  textArea.style.opacity = "0";
  document.body.appendChild(textArea);
  textArea.select();
  const copied = document.execCommand("copy");
  textArea.remove();

  if (!copied) throw new Error("Clipboard copy failed");
}

document.querySelectorAll("[data-close-modal]").forEach((button) => {
  button.addEventListener("click", async () => {
    if (button.hasAttribute("data-copy-demo")) {
      try {
        const command = await loadDemoCommand();
        await copyText(command);
        showToast("검증된 데모 명령이 클립보드에 복사되었습니다.");
      } catch {
        showToast("클립보드 복사에 실패했습니다.");
      }
    }

    closeModal(button.closest(".modal-backdrop"));
  });
});

document.querySelectorAll(".modal-backdrop").forEach((modal) => {
  modal.addEventListener("click", (event) => { if (event.target === modal) closeModal(modal); });
});

document.addEventListener("keydown", (event) => {
  if (event.key === "Escape") document.querySelectorAll(".modal-backdrop:not([hidden])").forEach(closeModal);
});

document.querySelectorAll("[data-toast]").forEach((button) => {
  button.addEventListener("click", () => showToast(button.dataset.toast));
});
