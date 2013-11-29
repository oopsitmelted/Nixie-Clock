EESchema Schematic File Version 2  date 8/9/2011 8:21:40 PM
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:special
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:nixie
LIBS:supertex
LIBS:nxp
LIBS:AtomicNixieRevB-cache
EELAYER 25  0
EELAYER END
$Descr A4 11700 8267
encoding utf-8
Sheet 1 3
Title "Atomic Nixie"
Date "10 aug 2011"
Rev "B"
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Wire Wire Line
	5550 3250 6350 3250
Wire Wire Line
	5550 3050 6350 3050
Wire Wire Line
	5550 2950 6350 2950
Wire Wire Line
	5550 3150 6350 3150
Wire Wire Line
	5550 3350 6350 3350
$Sheet
S 6350 2650 1900 1200
U 4DCCADB7
F0 "Nixie Drivers" 60
F1 "nixie_driversRevB.sch" 60
F2 "BLANK" I L 6350 2950 60 
F3 "POL" I L 6350 3050 60 
F4 "LE" I L 6350 3150 60 
F5 "CLK" I L 6350 3250 60 
F6 "DIN" I L 6350 3350 60 
$EndSheet
$Sheet
S 3750 2650 1800 1200
U 4DCCAE01
F0 "Power Supply and CPU" 60
F1 "power_supply_cpuRevB.sch" 60
F2 "NIXIE_DIN" O R 5550 3350 60 
F3 "NIXIE_CLK" O R 5550 3250 60 
F4 "NIXIE_LE" O R 5550 3150 60 
F5 "NIXIE_POL" O R 5550 3050 60 
F6 "NIXIE_BLANK" O R 5550 2950 60 
$EndSheet
$EndSCHEMATC
