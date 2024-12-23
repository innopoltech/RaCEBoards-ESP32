import random
import time
import struct
import serial

#Global
file_list = []
chunks_data = bytearray()
rec_data = bytearray()
full_size = 0
prev_time = 0
time_start = 0
byte_sent = 0
id = 0
read_max_size = 0
read_frag = 0
ping_ok = False

#Uart comm ported from microblock IDE
#https://bitbucket.org/john_maloney/smallvm/src/stable/misc/SERIAL_PROTOCOL.md

COM = ""
device = None
def set_COM(COM_):
    global COM, device
    COM = COM_ 

    device = serial.Serial(COM, 115200)
    device.set_buffer_size(8*1024*1024, 8*1024*1024)



Ping                = 0x1A
DeleteFileMsg       = 200
ListFilesMsg        = 201
FileInfoMsg         = 202
StartReadingFileMsg = 203
StartWritingFileMsg = 204
FileChunkMsg        = 205

def write_short(cmd, chunk_id):
    msg = bytearray()
    msg.append(0xFA)
    msg.append(cmd)
    msg.append(chunk_id)
    msg.append(0xFE)

    device.write(msg)

def write_long(cmd, chunk_id, data):
    msg = bytearray()
    msg.append(0xFB)
    msg.append(cmd)
    msg.append(chunk_id)

    data_len = len(data) + 1
    msg.append(data_len & 0xFF)
    msg.append((data_len >> 8) & 0xFF)
    
    msg.extend(data)
    msg.append(0xFE)

    device.write(msg)

def read_wtimeout(timeout=1):
    global rec_data
    lastRcv = time.time()

    while (time.time() - lastRcv) < timeout:
        data = device.read(device.in_waiting)
        if(len(data) >0):
            #timeout = 0.5 # decrease timeout after first response
            
            #Process
            rec_data.extend(data)
            rec_data, exit_ = processMessages(rec_data)
            if(exit_):
                return
            lastRcv = time.time()
        time.sleep(0.01)
    
    #After timeout process
    for i in range(100):
        rec_data, exit_= processMessages(rec_data)
        if(exit_):
            return

def handle(msg):
    global file_list, chunks_data, read_frag, read_max_size, prev_time, full_size, ping_ok

    op = msg[1]

    if(op == Ping):
        ping_ok = True
        return True

    elif(op == FileInfoMsg):
        msg = msg[5:]

        #Add to local list
        fileNum = struct.unpack("<i" , bytearray(msg[0:4]))[0]
        fileSize = struct.unpack("<i" , bytearray(msg[4:8]))[0]
        try:
            fileName = bytearray(msg[8:]).decode()
        except Exception:
            print("Invalid file name - delete file! Name=", msg[8:])
            write_long(DeleteFileMsg, 0, bytearray(msg[8:]))
            time.sleep(0.2)
            #mlistdir("") 
            return False
        file_list.append([fileNum, fileName, fileSize])

    elif(op == FileChunkMsg):
        msg = msg[5:]

        #Calc dt
        dt = time.time() - prev_time
        prev_time = time.time()

        #Get param
        id = struct.unpack("<i" , bytearray(msg[0:4]))[0]
        offset = struct.unpack("<i" , bytearray(msg[4:8]))[0]
        data = msg[8:]
        len_now = offset + len(data)
        percent = (100.0/full_size) * len_now
      
        if(len(data) > 0):
            print(f"    RX     : dT={dt*1000:8.4f} ms, size={len(data):8} byte. Progress - {percent:3.2f}% ({len_now}/{full_size} bytes)")
            chunks_data.extend(data)

            if(read_max_size != 0 and len(chunks_data)>read_max_size):
                read_frag = chunks_data[:read_max_size]
                chunks_data = chunks_data[read_max_size:]
                return True
        else:
            print(f"    RX done: dT={dt*1000:8.4f} ms, total size={full_size:8} byte.")
            return True
    else:
        print ("Ignore msg:", bytearray(msg).hex())

    return False

def discardMessage(msg):
    for i in range(1, len(msg)):
        if(msg[i] == 250 or msg[i] ==251):
            return msg[i:]
    return bytearray()

def processMessages(msg):
    if(msg == None):
        return bytearray(), False

    if(len(msg) <3):
        return msg, False

    #Parse and dispatch messages
    firstByte = msg[0]
    byteTwo = msg[1] 
    if (byteTwo != Ping and (byteTwo < DeleteFileMsg or byteTwo > FileChunkMsg)): # Ping and FileTransfer
        print ("Serial error, opcode:", msg[2])
        msg = discardMessage(msg)
        return msg, False

    if (250 == firstByte): # short message
        h_msg = msg[:3]
        msg = msg[3:] # remove message
        return msg, handle(h_msg)
    elif(251 == firstByte): # long message
        if(len(msg) < 5):
            return msg, False

        bodyBytes = msg[4] << 8 |  msg[3]
        if (bodyBytes >= 1024):
            print("Serial error, length:", bodyBytes)
            msg = discardMessage(msg)
            return msg, False

        if len(msg) < (5 + bodyBytes):
            return msg, False # incomplete body

        h_msg = msg[:bodyBytes + 5]
        msg = msg[bodyBytes + 5:] # remove message
        return msg, handle(h_msg)

    else:
        print ("Serial error, start byte:", firstByte)
        msg = discardMessage(msg)
        return msg, False
    
#files
class VirtualFile:
    '''
    Virtual file
    '''
    def __init__(self, path, modf, is_write=False):
        self.path = path
        self.is_write = is_write
        self.closed = False
        self.eof = False
        self.name = path #No folders

        #Start read/write
        if(is_write):
            mwrite(0, path, None)
        else:
            mread(0, path, 0)

    def write(self, data):
        if(self.is_write):
            mwrite(1, self.path, data)

    def read(self, max_size):
        #override producer buffer size
        max_size = min(2048, max_size)
        if(not self.is_write and self.eof == False):
           data, self.eof = mread(1, self.path, max_size)
           return data
        else:
            return bytearray()

    def close(self):
        if(self.is_write):
            mwrite(2, self.path, None)
        else:
            mread(2, self.path, 0)
        self.closed = True

def mlistdir(path):
    global file_list
    #Clear local list
    file_list = []

    #Update local list
    write_short(ListFilesMsg, 0) #List Files
    read_wtimeout()

    #Formatting to ftp format
    list_files = []
    for file in file_list:
        list_files.append(file[1])  #name

    print("  Update local list:")
    for file in file_list:
        print(f"    {file[0]:3}: size={file[2]:8} bytes, name= '{file[1]}'")

    return list_files

def mremove(path):
    #Delete '/'
    filename_ = path[1:] if path[0] == '/' else path

    #Fill name
    name = bytearray()
    name.extend(filename_.encode())
    write_long(DeleteFileMsg, 0, name)

    #Update local list
    mlistdir("") 

def mread(cmd, filename, max_size):
    global chunks_data, time_start, read_frag, read_max_size, prev_time, full_size

    #Start RX
    if(cmd == 0):
        #Clear input buffer
        while(len(rec_data) > 0):
            read_wtimeout()

        #Fill id and name
        id = random.randint(1, (1 << 24) - 1)
        data = bytearray()
        data.extend(id.to_bytes(4, "little"))
        data.extend(filename.encode())
        
        #Send and wait answer
        prev_time = time.time()
        time_start = time.time()
        full_size = mgetsize(filename)
        chunks_data = bytearray()

        read_max_size = 0
        read_frag = bytearray()

        print(f"  Start read file, name = '{filename}', size = {full_size} bytes:")
        write_long(StartReadingFileMsg, 0, data)

    #RX data
    elif(cmd == 1):
        read_max_size = max_size
        read_frag = bytearray()
        read_wtimeout()
        
        eof = False
        if(len(read_frag) > 0): #Frag data
            ret = read_frag[:]
            read_frag = bytearray()
            eof = False
        else:                   #No-Frag data
            ret = chunks_data[:]
            chunks_data = bytearray() 
            eof = True
        return ret, eof
    
    #End RX
    elif(cmd == 2):
        read_max_size = 0
        read_wtimeout()

        print(f"  End read file, name = '{filename}', size = {full_size} bytes! Total time = {(time.time() - time_start)*1000:.2f} ms")
        return None

def mwrite(cmd, filename, file_data):
    global id, ping_ok, prev_time, time_start, byte_sent, rec_data
    
    #Start TX
    if(cmd == 0):
        #Clear input buffer
        while(len(rec_data) > 0):
            read_wtimeout()

        #Fill id and name
        id = random.randint(1, (1 << 24) - 1)
        data = bytearray()
        data.extend(id.to_bytes(4, "little"))
        data.extend(filename.encode())

        #Send
        prev_time = time.time()
        time_start = time.time()
        byte_sent = 0

        print(f"  Start write file, name = '{filename}', size = ? bytes:")
        write_long(StartWritingFileMsg, 0, data)

    #TX data
    elif(cmd == 1):
        #Chunks with data
        local_offset = 0
        new_edge = byte_sent + len(file_data)

        while(byte_sent < new_edge):
            data = bytearray()
            data.extend(id.to_bytes(4, "little"))
            data.extend(byte_sent.to_bytes(4, "little"))

            max_chunk_size = min(500, (new_edge - byte_sent))
            data.extend( file_data[local_offset: local_offset + max_chunk_size ] )
            write_long(FileChunkMsg, 0, data)

            #Ping
            ping_ok = False
            write_short(Ping, 0)
            read_wtimeout()
            if(not ping_ok):
                print("    TX     : no ping from device!")
                continue    #Rery?

            #Calc and print
            byte_sent    += max_chunk_size
            local_offset += max_chunk_size
            dt = time.time() - prev_time
            prev_time = time.time()
            print(f"    TX     : dT={dt*1000:8.4f} ms, size={max_chunk_size:8} byte. Progress - ?% ({byte_sent}/? bytes)")
    
    #End TX
    elif(cmd == 2):
        #Empty chunk
        data = bytearray()
        data.extend(id.to_bytes(4, "little"))
        data.extend(byte_sent.to_bytes(4, "little"))

        write_long(FileChunkMsg, 0, data)

        #Ping
        for i in range(5):
            ping_ok = False
            write_short(Ping, 0)
            read_wtimeout()
            if(not ping_ok):
                print("    TX     : no ping from device!")
                continue
            else:
                break

        #Clear input buffer
        while(len(rec_data) > 0):
            read_wtimeout()

        #Calc and print
        dt = time.time() - prev_time
        print(f"    TX done: dT={dt*1000:8.4f} ms, total size={byte_sent:8} byte.")
        print(f"  End write file, name = '{filename}', size = {byte_sent} bytes! Total time = {(time.time() - time_start)*1000:.2f} ms")

        #Update local list
        mlistdir("")

#Helpers
def misfile(path):
    return "." in path
    
def misdir(path):
    return not misfile(path)

def mgetsize(path):
    global file_list

    #Delete '/'
    filename_ = path[1:] if path[0] == '/' else path

    for file in file_list:
        if(file[1] == filename_):
            return file[2]
    return 0

def mopen(filename:str, mode):
    global file_list

    #Delete '/'
    filename_ = filename[1:] if filename[0] == '/' else filename

    #Read
    if('r' in mode):
        #Check file in local list
        found = False
        for file in file_list:
            if(file[1] == filename_):
                found = True
                break
        if(not found):
            raise OSError("mopen - file not found")

        file = VirtualFile(filename_, "rb",is_write=False)
        return file
    
    #Write
    if('w' in mode):
        file = VirtualFile(filename_, "wb", is_write=True)
        return file