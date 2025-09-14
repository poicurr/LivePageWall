# WebView2 Desktop Wallpaper(Demo)

技術検証用の最小サンプル。WebView2 で任意のページ/HTMLをデスクトップ壁紙として表示する。

## Requirements
- Windows 10/11
- WebView2 SDK（`1.download_webview2.bat`で取得）
- Microsoft C/C++ コンパイラ `cl.exe` が使用可能であること

## Quick Start
1. WebView2 を導入  
   `1.download_webview2.bat`
2. ビルド（cl.exe 必須）  
   `2.build.bat`
3. 起動
   - URL を壁紙化: `run-url.bat`
   - ローカルHTML を壁紙化: `run-file1.bat` / `run-file2.bat`
4. 停止  
   `kill.bat`
