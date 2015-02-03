package main

import (
	"log"
	"os"

	"github.com/tarm/goserial"
)

const nLEDs = 92

func main() {
	// Open the serial port
	c := &serial.Config{Name: os.Args[1], Baud: 115200}
	ser, err := serial.OpenPort(c)
	if err != nil {
		log.Fatalln("Trouble opening serial: ", err)
	}

	buf := make([]byte, (nLEDs*3 + 1))
	buf[0] = 0x0F
	for i := 0; i < nLEDs; i++ {
		buf[1+i*3], buf[2+i*3], buf[3+i*3] = 0x80, 0x80, 0x80
	}

	ser.Write(buf)
}
