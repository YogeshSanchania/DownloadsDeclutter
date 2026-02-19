# Downloads Declutter 🧹

A lightweight, high-performance Windows utility to organize your chaotic Downloads folder instantly. Built with pure **Win32 C++** for zero dependencies and maximum speed.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-0078d7.svg)
![Language](https://img.shields.io/badge/language-C++17-f34b7d.svg)

## 🚀 Features

* **Zero Dependencies:** Native Win32 API application. No .NET runtime or heavy frameworks required.
* **High Performance:** Uses `std::thread` for non-blocking scans and **Virtual List View** to handle 100,000+ files instantly without UI freezing.
* **Smart Organization:** Automatically sorts files into `Images`, `Documents`, `Videos`, `Installers`, etc.
* **Safety First:**
    * **Undo Capability:** Accidentally moved something? One-click Undo restores everything exactly as it was.
    * **System Protection:** Automatically detects and refuses to touch system folders (e.g., `Windows`, `Program Files`).
    * **UTF-8 Support:** Fully supports Hebrew, Arabic, and other non-ASCII filenames.
* **Context Menu Integration:** Right-click any folder in Explorer -> "Scan for Declutter".

## 🛠️ Tech Stack

* **Language:** C++17
* **API:** Win32 API (User32, Shell32, Shlwapi, Comctl32)
* **Architecture:** Multi-threaded Worker/UI model
* **Visual Styles:** Modern Windows Common Controls (v6.0 manifest)

## 📦 How to Build

1.  Open the solution in **Visual Studio 2022/2026**.
2.  Select **Release** and **x64** (or x86).
3.  Ensure the Runtime Library is set to **Multi-threaded (/MT)** to statically link the CRT (Properties -> C/C++ -> Code Generation -> Runtime Library).
4.  Build Solution (`Ctrl + Shift + B`).

## 💿 Installation

You can download the latest installer from the [Releases](https://github.com/yogeshsanchania/DownloadsDeclutter/releases) page, or build it yourself using the included `setup_script.iss` with Inno Setup.

## ☕ Support the Developer

If this tool saved you time or helped you learn Win32 programming, consider buying me a coffee!

<a href="https://buymeacoffee.com/yogeshsanchania" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>

---
*Developed by [Yogesh](https://www.linkedin.com/in/yogesh-sanchania)*