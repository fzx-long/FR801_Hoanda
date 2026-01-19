# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['main.py'],
    pathex=[],
    binaries=[],
    datas=[('C:\\Users\\14569\\Desktop\\五羊本田\\software\\FR801xH-SDK\\FR801xH-SDK\\examples\\none_evm\\ble_simple_peripheral\\code\\serial_assistant\\resources', 'resources'), ('C:\\Users\\14569\\Desktop\\五羊本田\\software\\FR801xH-SDK\\FR801xH-SDK\\examples\\none_evm\\ble_simple_peripheral\\code\\serial_assistant\\data', 'data')],
    hiddenimports=['PyQt5.QtWidgets', 'PyQt5.QtCore', 'PyQt5.QtGui', 'serial', 'serial.tools.list_ports'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='串口测试助手',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
