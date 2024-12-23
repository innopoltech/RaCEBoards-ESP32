
try:
    from ftp_server import start_ftp_server
    from ftp_microblock_files import *
except Exception:
    from .ftp_server import start_ftp_server
    from .ftp_microblock_files import *

#Старт
set_COM("COM6")
start_ftp_server()