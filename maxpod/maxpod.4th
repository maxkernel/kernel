( *********************************************************************
( *    Created by Andrew Klofas - aklofas@gmail.com - August 2009     *
( *  Origional use for Max operation on MiniPod, may be ported later  *
( *********************************************************************

( ---------------------------- CONSTANTS ------------------------------------ )
( Version number
DECIMAL
09 CONSTANT	VERSION			EEWORD

( CPU frequency
HEX
4B40 4C DCONSTANT CPU-FREQ		EEWORD

( State machine refresh freq
DECIMAL
100 CONSTANT	STATE-CYCLE-FREQ	EEWORD

( Allocate 256 bytes for serial Rx and Tx Buffers
DECIMAL
HERE 1500 ALLOT CONSTANT RXBUFF EEWORD 
HERE 1500 ALLOT CONSTANT TXBUFF EEWORD

( Width, in bytes, of commands
DECIMAL
3  CONSTANT	CMDWIDTH		EEWORD

( RX format bytes
DECIMAL
40  CONSTANT RX_DLE EEWORD
41  CONSTANT RX_STX EEWORD
126 CONSTANT RX_ETX EEWORD

( RX command modes
HEX
0  CONSTANT MODE_DLE EEWORD
1  CONSTANT MODE_CMD EEWORD

( Opcodes
HEX
0  CONSTANT 	CMD_HEARTBEAT 		EEWORD
1  CONSTANT	CMD_ECHO		EEWORD
2  CONSTANT	CMD_PWM			EEWORD	( 2 through 7 are pwm opcodes (6 pwm pins)
( 3 4 5 6 7 are pwm opcodes too
8  CONSTANT	CMD_PWMOFF		EEWORD
9  CONSTANT	CMD_PWMALLOFF		EEWORD
A  CONSTANT	CMD_SETPOLL		EEWORD
B  CONSTANT	CMD_ERROR		EEWORD	( CMD_ERROR should always be the last/highest cmd

( Poll Opcodes
HEX
0  CONSTANT	POLL_FAILSAFE		EEWORD
1  CONSTANT	POLL_HEARTBEAT		EEWORD

( Error codes
HEX
0  CONSTANT	ERROR_UNKNOWN_CMD	EEWORD	( Unknown command recieved
1  CONSTANT	ERROR_MALFORMED_CMD	EEWORD	( Malformed command recieved
2  CONSTANT	ERROR_PARITY_FAILURE	EEWORD	( Parity check didn't pass
3  CONSTANT	ERROR_OUT_OF_SEQUENCE	EEWORD	( Recieved an unexpected
						( [not DLE or STX] byte between commands
DECIMAL
50 CONSTANT	FAILSAFE_TIMER_END	EEWORD	( 1/2 sec to timer end with 10 msec step
25 CONSTANT	HEARTBEAT_TIMER_END	EEWORD	( 1/4 sec to timer end with 10 msec step


( --------------------------- VARIABLES ------------------------------------- )

( Failsafe timer: once timer reaches end and no contact from cpu,
( SHUTDOWN is called
LOOPINDEX FAILSAFE_TIMER EEWORD
DECIMAL FAILSAFE_TIMER_END FAILSAFE_TIMER END

( Heartbeat timer: once timer reaches end, send heartbeat
LOOPINDEX HEARTBEAT_TIMER EEWORD
DECIMAL HEARTBEAT_TIMER_END HEARTBEAT_TIMER END

( RX command mode
VARIABLE RXMODE EEWORD

( Num of bytes recieved from serial RX
VARIABLE RXBYTES EEWORD

( True until serial activity. Used for failsafe cuttoff
VARIABLE IDLE EEWORD

( Recieved opcode from RX (opcode and cmd are used interchangeably)
VARIABLE OPCODE EEWORD


( --- , Turns all pwm ports off
: PWMALLOFF
	( Turn off all PWMA's (will auto-turnon when set)
	PWMA0 OFF
	PWMA1 OFF
	PWMA2 OFF
	PWMA3 OFF
	PWMA4 OFF
	PWMA5 OFF
; EEWORD

( --- , Initialize board, to be called before anything else
: INIT
	
	( Link serial buffers to software UART
	DECIMAL
	RXBUFF  1500 SCI0 RXBUFFER
	TXBUFF  1500 SCI0 TXBUFFER
	
	( Specify PWM Freq (76 Hz = 2500000 / 32767)
	( Applies to all PWMA's
	DECIMAL 32767 PWMA0 PWM-PERIOD
	( DECIMAL 50000 PWMA0 PWM-PERIOD
	
	( Make all PWMA's independent of each other
	PWMA0 INDEPENDENT
	PWMA2 INDEPENDENT
	PWMA4 INDEPENDENT
	
	PWMALLOFF	( Turn all pwm's off
	
; EEWORD


( CMD P1 P2 --- , Send command through serial with two parameters (postfix)
( FIXME - Add more comments
: SNDCMD
	SWAP	( Swap P1 and P2
	
	2 PICK
	2 PICK
	2 PICK
	
	+ +
	( TODO - don't mod (just let overrun
	256 MOD
	
	3 -ROLL	
	
	RX_STX
	SCI0 TX
	TX
	TX
	TX
	TX
	RX_ETX TX
; EEWORD

( --- , Outputs 7 DLE's through the serial. This is used to sync
(         the output in case of error
: SNDDLE
	RX_DLE RX_DLE RX_DLE RX_DLE RX_DLE RX_DLE RX_DLE
	SCI0 TX TX TX TX TX TX TX
; EEWORD


( ------------------------- FAILSAFE MACHINE --------------------------------- )
( Failsafe machine. If no contact from cpu in FAILSAFE_TIMEOUT
( then the minipod should assume cpu froze. In that case, call
( SHUTDOWN, which turns off all PWM's and uninstall all state machines
MACHINE FAILSAFE EEWORD
ON-MACHINE FAILSAFE
	APPEND-STATE CHK-FAILSAFE EEWORD

( If failsafe timer end reached and initial greeting sent
( Do turn redled on, green led off then call shutdown
( Then (do nothing)
IN-STATE CHK-FAILSAFE
	CONDITION FAILSAFE_TIMER COUNT IDLE @ NOT AND
	CAUSES
		YELLED ON
		GRNLED OFF
		PWMALLOFF
		CMD_PWMALLOFF 0 0 SNDCMD
		TRUE IDLE !
	THEN-STATE CHK-FAILSAFE NEXT-TIME IN-EE



( ------------------------- HEARTBEAT MACHINE -------------------------------- )
( Heartbeat machine. This will send a heartbeat frame through the
( serial every 1 sec (or whatever end is specified in HEARTBEAT_TIMEOUT)
MACHINE HEARTBEAT EEWORD
ON-MACHINE HEARTBEAT
	APPEND-STATE SEND-HEARTBEAT EEWORD

( If heartbeat timer end reached
( Do send heartbeat frame and toggle red led
( Then repeat
IN-STATE SEND-HEARTBEAT
	CONDITION HEARTBEAT_TIMER COUNT
	CAUSES
		CMD_HEARTBEAT 0 0 SNDCMD
		REDLED TOGGLE
	THEN-STATE SEND-HEARTBEAT NEXT-TIME IN-EE


( ------------------------- RECEIVER MACHINE --------------------------------- )
( Serial receiver machine. This will read three bytes from the serial:
( First byte = Param 1
( Second byte = Param 2
( Third byte = Command/Opcode
( Once three bytes have been received, this machine will execute the command
( and respond appropriately
MACHINE RECEIVER EEWORD
ON-MACHINE RECEIVER
	APPEND-STATE RECEIVE-RX	EEWORD
	APPEND-STATE CHK-RX	EEWORD
	APPEND-STATE CHK-ETX	EEWORD
	APPEND-STATE CHK-PARITY	EEWORD
	APPEND-STATE EXE-CMD	EEWORD

( If received RX
( Do push RX to stack, set IDLE variable to FALSE, and reset failsafe timer
( Then goto state CHK-RX
IN-STATE RECEIVE-RX
	CONDITION SCI0 RX?
	CAUSES
		SCI0 RX
		FAILSAFE_TIMER RESET
		YELLED OFF
		FALSE IDLE !
	THEN-STATE CHK-RX THIS-TIME IN-EE


( If we are in mode MODE_DLE
( Do send error if received byte is not RX_DLE or RX_STX, then put in
(    MODE_CMD mode [start the cmd recieving process] if received RX_STX
(    otherwise we have received an RX_DLE, which is just padding,
(    so we do nothing
( Then goto state RECEIVE-RX
IN-STATE CHK-RX
	CONDITION RXMODE @ MODE_DLE =
	CAUSES
		DUP RX_STX = IF			( if received byte is RX_STX
			MODE_CMD RXMODE !	( put in MODE_CMD mode (start recieving cmds)
			0 RXBYTES !		( reset byte counter
		ELSE
			DUP RX_DLE = NOT IF	( if revieved byte is other than RX_DLE and RX_STX
				CMD_ERROR ERROR_OUT_OF_SEQUENCE ROT SNDCMD
						( send error command containing OPCODE = CMD_ERROR
						( PARAM1 = ERROR_OUT_OF_SEQUENCE
						( PARAM2 = the received illegal byte
				0		( put zero on stack so the following drop command
						( has something to drop
			THEN
		THEN
		DROP
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If we are in MODE_CMD mode and we haven't yet received 5 bytes
( Do add one to RXBYTES and leave received byte on stack
( Then goto state RECEIVE-RX
IN-STATE CHK-RX
	CONDITION RXMODE @ MODE_CMD = RXBYTES @ 4 < AND
	CAUSES
		RXBYTES 1+!
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If we are in MODE_CMD mode and we have recieved 5 bytes
( Do nothing
( Then goto state CHK-ETX
IN-STATE CHK-RX
	CONDITION RXMODE @ MODE_CMD = RXBYTES @ 4 = AND
	CAUSES
	THEN-STATE CHK-ETX THIS-TIME IN-EE

( If the last byte received is *not* RX_ETX, i.e. we haven't received the
(     expected end-of-transmition byte
( Do drop the 5 received bytes, put us back in MODE_DLE and respond
(     with appropriate error
( Then goto state RECEIVE-RX
IN-STATE CHK-ETX
	CONDITION DUP RX_ETX = NOT
	CAUSES
		DROP DROP DROP DROP DROP		( drop the 5 received bytes off stack
		MODE_DLE RXMODE !			( put me back in MODE_DLE mode
		CMD_ERROR ERROR_MALFORMED_CMD 0 SNDCMD	( send error [malformed] command
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If the last byte received is RX_ETX
( Do drop RX_ETX byte, then calculate parity, leaving the parity-success
(     bool on stack
( Then goto state CHK-PARITY
IN-STATE CHK-ETX
	CONDITION DUP RX_ETX =
	CAUSES
		DROP			( drop the ETX
		
		3 PICK			( copy param1, param2, cmd
		3 PICK			
		3 PICK
		
		+ +			( do the sum parity check
		256 MOD			( [param1 + param2 + cmd] % 256 should equal the parity byte
		
		=			( test for sent-parity == calculated-parity
	THEN-STATE CHK-PARITY THIS-TIME IN-EE

( If the parity-success bool is false
( Do drop the 4 accumulated bytes off stack, set mode to MODE_DLE,
(     and respond with appropriate error
( Then goto state RECEIVE-RX
IN-STATE CHK-PARITY
	CONDITION DUP NOT
	CAUSES
		DROP DROP DROP DROP			( drop 4 [cmd + parity bool] bytes off stack
		MODE_DLE RXMODE !			( put me back in MODE_DLE mode
		CMD_ERROR ERROR_PARITY_FAILURE 0 SNDCMD	( send error [parity failure] response
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If the parity-success bool to true
( Do drop parity-success bool off of stack, tidy command for easy
(     access, then set mode to MODE_DLE
( Then goto state EXE-CMD
IN-STATE CHK-PARITY
	CONDITION DUP
	CAUSES
		DROP					( drop parity-success bool off stack
		OPCODE !				( assign opcode ontop of stack to OPCODE var
		SWAP					( swap cmd params so param1 is top of stack
		MODE_DLE RXMODE !			( put me back in MODE_DLE mode
	THEN-STATE EXE-CMD THIS-TIME IN-EE

( If OPCODE is a HEARTBEAT command
( Do drop its two useless parameters and do nothing
( Then goto RECEIVE-RX
IN-STATE EXE-CMD
	CONDITION OPCODE @ CMD_HEARTBEAT =
	CAUSES
		DROP DROP
		GRNLED TOGGLE
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If OPCODE is a CMD_ECHO
( Do echo back the command [including parameters]
( Then goto RECEIVE-RX
IN-STATE EXE-CMD
	CONDITION OPCODE @ CMD_ECHO =
	CAUSES
		SWAP			( swap parameters so stack is param1, param2 [top]
		CMD_ECHO 2 -ROLL	( insert a CMD_ECHO so stack is cmd, param1, param2 [top]
		SNDCMD			( command out!
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If OPCODE >= CMD_PWM && OPCODE <= CMD_PWM+6 (means that it is a pwm command)
( Do pack the two 8 bit parameters to one 16 bit parameter, then populate
(     PWM-SELECT with the selected pwm
( Then goto RECEIVE-RX
IN-STATE EXE-CMD
	CONDITION OPCODE @ CMD_PWM 1 - > OPCODE @ CMD_PWM 6 + < AND
	CAUSES
		DECIMAL 256 * +			( pack two parameters into 1 number where P1 is MSB
		OPCODE @ CMD_PWM -		( get selected pwm (0 - 5)
		
		DUP 0 = IF PWMA0 ELSE		( a bunch of if statements because there is no (known)
		DUP 1 = IF PWMA1 ELSE		( way of selecting the proper pwm port. i tried
		DUP 2 = IF PWMA2 ELSE		( getting the addr of PWMA0 (which is 3D1D on minipod)
		DUP 3 = IF PWMA3 ELSE		( then using that as a base to find the desired port
		DUP 4 = IF PWMA4 ELSE		( address (which goes up by steps of 7)
		DUP 5 = IF PWMA5		( but failed because there is no way of telling forth
		THEN THEN THEN THEN THEN THEN	( to select the port by giving it the port addr
		
		DROP				( drop the origional port num from the stack
		PWM-OUT				( send the pwm to the selected port
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If OPCODE is a PWMOFF [turn off pwm port on param] command
( Do determine which pwm is to be turned off, then turn it off
( Then goto RECEIVE-RX
IN-STATE EXE-CMD
	CONDITION OPCODE @ CMD_PWMOFF =
	CAUSES
		DUP 0 = IF PWMA0 ELSE		( select the right pwm to turn off
		DUP 1 = IF PWMA1 ELSE
		DUP 2 = IF PWMA2 ELSE
		DUP 3 = IF PWMA3 ELSE
		DUP 4 = IF PWMA4 ELSE
		DUP 5 = IF PWMA5
		THEN THEN THEN THEN THEN THEN
		
		OFF				( turn that pwm off
		DROP DROP			( drop the two params
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If OPCODE is a PWMALLOFF [turn all pwm ports off] command
( Do first drop its two useless params, then call PWMALLOFF function
( Then goto RECEIVE-RX
IN-STATE EXE-CMD
	CONDITION OPCODE @ CMD_PWMALLOFF =
	CAUSES
		DROP DROP			( drop the two params
		PWMALLOFF			( call function to stop all pwms
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If OPCODE is a SETPOLL command
( Do select the timer based on the first parameter. If the first param is
(     out of range, send a ERROR_MALFORMED_CMD error and set the second param
(     to 101 to ensure that no timer will be set. Once timer is selected, ensure
(     that second param, freq, is from 0 to 100, otherwise do nothing. If that
(     test succedes, find the ticks and set/reset the timer
( Then goto RECEIVE-RX
IN-STATE EXE-CMD
	CONDITION OPCODE @ CMD_SETPOLL =
	CAUSES
		DUP POLL_FAILSAFE = IF		FAILSAFE_TIMER		ELSE
		DUP POLL_HEARTBEAT = IF		HEARTBEAT_TIMER		ELSE
		
		CMD_ERROR ERROR_MALFORMED_CMD CMD_SETPOLL SNDCMD
		SWAP DROP 101 SWAP
		
		THEN THEN
		
		DROP
		DUP DUP 101 < SWAP -1 > AND IF			( if freq is between 0 and 100
			DUP IF					( if freq is zero, don't divide
				STATE-CYCLE-FREQ SWAP /
			THEN
			
			END					( set the end and
			RESET					( reset the timer
		THEN
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If OPCODE is a CMD_ERROR
( Do if error is a ERROR_MALFORMED_CMD, output a bunch of DLE,
(     otherwise ignore
( Then goto RECEIVE-RX
IN-STATE EXE-CMD
	CONDITION OPCODE @ CMD_ERROR =
	CAUSES
		DUP ERROR_MALFORMED_CMD = IF
			SNDDLE			( Send syncing DLE's
		ELSE
			( Do nothing, only take action on ERROR_MALFORMED_CMD
		THEN
		
		DROP DROP
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE

( If OPCODE is > CMD_ERROR (means that it is out of valid command range)
( Do first drop its two unknown parameters, then send an appropriate
(     error message back (ERROR_UNKNOWN_CMD)
( Then goto RECEIVE-RX
IN-STATE EXE-CMD
	CONDITION OPCODE @ CMD_ERROR >
	CAUSES
		DROP DROP
		CMD_ERROR ERROR_UNKNOWN_CMD OPCODE @ SNDCMD
	THEN-STATE RECEIVE-RX THIS-TIME IN-EE
	
	



( --- , Hangs foreground indefinitely
: HANG
	BEGIN 0 UNTIL
; EEWORD
		

( --- , Startup routine, calls INIT then installs all state machines
: MAIN
	
	ISOMAX-START			( Start ISOMAX
	INIT				( Call init routine
	TRUE IDLE !			( Set initial greeting flag from cpu to false
	
	FAILSAFE_TIMER RESET		( Reset failsafe timer
	CHK-FAILSAFE SET-STATE		( Set CHK-FAILSAFE as initial state
	INSTALL FAILSAFE		( Install failsafe state machine
	
	HEARTBEAT_TIMER RESET		( Reset heartbeat timer
	SEND-HEARTBEAT SET-STATE	( Set SEND-HEARTBEAT as initial state
	INSTALL HEARTBEAT		( Install heartbeat state machine
	
	0 RXBYTES !			( Set RXBYTES to 0 (num of bytes recieved in rx)
	RECEIVE-RX SET-STATE		( Set RECIEVE-RX as initial state
	INSTALL RECEIVER		( Install reciever state machine
	
	REDLED ON			( Show that everything has init and awaiting input
	GRNLED OFF
	
	HANG				( Hang foreground
; EEWORD

AUTOSTART MAIN		( Make MAIN autostart when powered up
SAVE-RAM		( Save words to flash-RAM
