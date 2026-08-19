@echo off
title Khoi dong Dashboard
echo ==============================================
echo   HE THONG DASHBOARD - DO AN TOT NGHIEP
echo ==============================================
echo.
echo Dang khoi dong Local Server tai cong 3000...
echo.

:: Kiem tra xem Node.js co hoat dong khong
where npx >nul 2>nul
if %errorlevel% neq 0 (
    echo [LOI] Khong tim thay Node.js / npx tren may tinh!
    echo Vui long cai dat Node.js truoc.
    pause
    exit
)

:: Đảm bảo chạy đúng ở thư mục hiện tại
cd /d "%~dp0"

:: Mở một luồng phụ chờ 3 giây rồi bật trình duyệt
start cmd /c "ping 127.0.0.1 -n 4 >nul && start http://localhost:3000"

echo.
echo [SERVER DANG CHAY] Vui long khong tat cua so nay trong suot buoi bao ve!
echo (Khi nao ban muon tat server thi chi can an dau X de dong cua so)
echo.

:: Chạy server trực tiếp trên cửa sổ này để dễ quan sát lỗi (nếu có)
npx serve -p 3000
