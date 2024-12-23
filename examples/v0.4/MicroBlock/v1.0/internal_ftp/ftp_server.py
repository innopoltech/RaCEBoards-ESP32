import socket
from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.filesystems import AbstractedFS, FilesystemError, _months_map
from pyftpdlib.handlers import FTPHandler, DTPHandler, PassiveDTP
from pyftpdlib.servers import FTPServer

import subprocess
import logging

try:
    from ftp_microblock_files import *
except Exception:
    from .ftp_microblock_files import *

#Enable full log
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger()

class myAbstractedFS():
    '''
    In the current version only actions on reading/writing the whole file, creating/deleting a file, getting the list of files of the root directory are supported.
    There are no folders.
    '''
    def __init__(self, root, cmd_channel):
        self.cwd = '/'
        self.root = root
        self.cmd_channel = cmd_channel

    ''' External interfaces '''
    def open(self, filename, mode):
        return mopen(filename, mode)

    def listdir(self, path):
        return mlistdir(path)

    def remove(self, path):
        mremove(path)

    def isfile(self, path):
        return misfile(path)

    def isdir(self, path):
        return misdir(path)

    def getsize(self, path):
        return mgetsize(path)

    def lexists(self, path):
        return self.isfile() or self.isdir()


    ''' Internal interfaces '''
    # --- Pathname / conversion utilities

    @staticmethod
    def _isabs(path):
        return path.startswith("/")

    def ftpnorm(self, ftppath):
        return "/"

    def ftp2fs(self, ftppath):
        if(len(ftppath)>0):
            if(ftppath[0] == '/'):
                return ftppath
            else:
                return f"/{ftppath}"
        return "/"

    def fs2ftp(self, fspath):
        return fspath

    def validpath(self, path):
        return (path.count('/') <=1 )

    # --- Wrapper methods around open() and tempfile.mkstemp

    def mkstemp(self, suffix='', prefix='', dir=None, mode='wb'):
        raise FilesystemError("mkstemp not implement")

    # --- Wrapper methods around os.* calls

    def chdir(self, path):
        pass

    def mkdir(self, path):
        raise FilesystemError("mkdir not implement")

    def listdirinfo(self, path):
        return self.listdir(path)

    def rmdir(self, path):
        raise FilesystemError("rmdir not implement")

    def rename(self, src, dst):
        raise FilesystemError("rename not implement")

    def chmod(self, path, mode):
        raise FilesystemError("chmod not implement")

    def stat(self, path):
        raise FilesystemError("stat not implement")

    def utime(self, path, timeval):
        raise FilesystemError("utime not implement")

    lstat = stat

    # --- Wrapper methods around os.path.* calls

    def islink(self, path):
        raise FilesystemError("islink not implement")

    def getmtime(self, path):
        raise FilesystemError("getmtime not implement")

    def realpath(self, path):
        return path

    # --- Listing utilities

    def format_list(self, basedir, listing, ignore_err=True):
        """ 
        Example output
        -rw-rw-rw-   1 owner   group    7045120 Sep 02  3:47 music.mp3
        drwxrwxrwx   1 owner   group          0 Aug 31 18:50 e-books
        -rw-rw-rw-   1 owner   group        380 Sep 02  3:40 module.py
        """
        answer = []
        for file in listing:
            #Select time source
            mtime = time.gmtime() if self.cmd_channel.use_gmt_times else time.localtime()
            #Fill time
            mtimestr = "%s %s" % (  # noqa: UP031
                    _months_map[mtime.tm_mon],
                    time.strftime("%d %H:%M", mtime),
                )
            #Fill size and name
            size = self.getsize(file)
            name = file

            #Create line
            line = "%s %3s %-8s %-8s %8s %s %s\r\n" % (
                "-rw-rw-rw-", "1", "owner", "group",
                size,
                mtimestr,
                name
            )
            answer.append(line[:])

        for ans in answer:
            yield ans.encode(self.cmd_channel.encoding, self.cmd_channel.unicode_errors)

    def format_mlsx(self, basedir, listing, perms, facts, ignore_err=True):
        raise FilesystemError("ftp_fs.py not implement")

class myPassiveDTP(PassiveDTP):
    ''' Limit TCP winsize - timeout bug'''
    #TCP speed ~unlimited, but uart speed ~10 kB/s. 
    #Need force slowdown tcp speed
    def create_socket(self, family=socket.AF_INET, type=socket.SOCK_STREAM):
        super().create_socket(family, type)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 10000)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 10000)    

class myFTPHandle(FTPHandler):
    ''' Not support change position '''
    def ftp_REST(self, file):
        self.respond("502 REST not implemented.")
    def ftp_APPE(self, file):
        self.respond("502 APPE not implemented.")

class myDTPHandler(DTPHandler):
    ''' Client timeout fix '''
    timeout_fix_count_write = 0

    def handle_read(self):
        self.cmd_channel.respond("150 Transfer in progress, please wait...\r\n")
        super().handle_read() #buffer 65k
    handle_read_event = handle_read

    def initiate_send(self):
        self.timeout_fix_count_write +=1
        if(self.timeout_fix_count_write % 20 == 0):
            self.cmd_channel.respond("150 Transfer in progress, please wait...\r\n")
        super().initiate_send() #buffer ~2-4k

def start_ftp_server():

    #Handlers
    handler = myFTPHandle
    handler.authorizer = DummyAuthorizer()
    handler.abstracted_fs = myAbstractedFS
    handler.passive_dtp = myPassiveDTP
    handler.dtp_handler = myDTPHandler
    handler.use_sendfile = False #Not support
    handler.timeout = 600        #Max time transfer ~4.8Mb

    #Microblock support only "lrdw"
    #Addition needed "e"
    handler.authorizer.add_user("user", "password", "/", perm="elrdw")

    #Start server
    server = FTPServer(("0.0.0.0", 21), handler)
    #server.max_cons         = 2 #Not support
    server.debug = True
    print("FTP-server started on port 21.")

    #Open client
    ftp_url = "ftp://user:password@127.0.0.1/"
    subprocess.run(["explorer", ftp_url])

    server.serve_forever()

if __name__ == "__main__":
    start_ftp_server()
