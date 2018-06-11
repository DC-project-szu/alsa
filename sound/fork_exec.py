#!/usr/bin/python

import os

def main():
    for i in range(1):
        pid = os.fork()
        if pid == 0:
            # os.execl('./open_pcm', './open_pcm', 'tone1.wav')
            os.system("write_pcm | open_pcm")
        os.wait()

if __name__ == '__main__':
    main()
