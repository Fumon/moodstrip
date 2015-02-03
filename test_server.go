package main

import (
	"log"
	"net"
)

const nLEDs = 92

func main() {
	// Open the serial port
	conn, err := net.Dial("tcp", "localhost:9996")
	if err != nil {
		log.Fatalln("Couldn't open localhost: ", err)
	}
	defer conn.Close()

	buf := make([]byte, (nLEDs*3 + 1))
	for j := 0; j < 15; j++ {
		buf[0] = 0x84
		for i := 0; i < nLEDs; i++ {
			buf[1+i*3], buf[2+i*3], buf[3+i*3] = 0x80, 0x80, 0x80
		}

		total := 0
		for total < (nLEDs*3 + 1) {
			n, err := conn.Write(buf[total:])
			if err != nil {
				log.Fatalln("Failed to write to connection: ", err)
			}
			total += n
			log.Println(total)
		}
	}
}
