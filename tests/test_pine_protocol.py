import socket
import struct
import sys
import threading
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from pcsx2_pine import Pine


class PineProtocolTests(unittest.TestCase):
    def test_fragmented_response_and_request_framing(self):
        client_socket,server_socket=socket.socketpair()
        client=Pine.__new__(Pine);client.socket=client_socket
        seen=[]
        def server():
            with server_socket:
                size=struct.unpack('<I',server_socket.recv(4))[0]
                seen.append(server_socket.recv(size-4))
                reply=struct.pack('<I',9)+b'\0'+struct.pack('<I',1)
                server_socket.sendall(reply[:2]);server_socket.sendall(reply[2:])
        thread=threading.Thread(target=server);thread.start()
        try:self.assertEqual(client.status(),'paused')
        finally:client.close();thread.join()
        self.assertEqual(seen,[b'\x0f'])

    def test_probe_extent_validation_before_io(self):
        client=Pine.__new__(Pine)
        for address,size in ((-1,1),(0,-1),(32*1024*1024,1)):
            with self.assertRaises(ValueError):client.read(address,size)


if __name__=='__main__':unittest.main()
