library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity main is
    Port ( 
        -- signals connected to MCU
        XPIO        : in  std_logic;        -- on rising edge start CMD     / STATUS   transfer (SREQ to L, wait for SACK)
        XDMA        : in  std_logic;        -- on rising edge start DATA IN / DATA OUT transfer (SREQ to L, wait for SACK)
        XMSG        : in  std_logic;        -- on rising edge start MSG  IN / MSG  OUT transfer (SREQ to L, wait for SACK)
        XRnW        : in  std_logic;        -- defines data direction (1: DATA1 <- DATA2,  0: DATA1 -> DATA2)
        XCMD        : out std_logic;        -- this is combination of CS and A1, will go low on 1st cmd byte from ACSI port
        reset_hans  : in  std_logic;        -- this is the signal which resets Hans, and it will reset this CPLD too to init it

        -- signals connected to MCU, which should be just copies of SCSI port states
        XRESET  : out std_logic;
        XCS     : out std_logic;
        XACK    : out std_logic;

        -- signals connected to SCSI port
        SRST : in    std_logic;            -- RESET device
        SEL  : in    std_logic;            -- target selection is marked with this signal
        BSYa : inout std_logic;            -- when device is listening
        BSYb : out   std_logic;            -- when device is listening
        DBPa : out   std_logic;            -- odd parity bit for data
        DBPb : out   std_logic;            -- odd parity bit for data

        IOa  : out   std_logic;            -- I/O - input / output
        IOb  : out   std_logic;            -- I/O - input / output
        CDa  : out   std_logic;            -- C/D - command / data
        CDb  : out   std_logic;            -- C/D - command / data
        SMSGa: out   std_logic;            -- MSG - message phase
        SMSGb: out   std_logic;            -- MSG - message phase

        SREQa: out   std_logic;            -- REQ goes from device...
        SREQb: out   std_logic;            -- REQ goes from device...
        SACK : in    std_logic;            -- ...and initiator answers with ACK

        -- DATA1 is connected to SCSI port, DATA2 is data latched on CS and ACK and connected to MCU
        DATA1a    : inout std_logic_vector(7 downto 0);
        DATA1b    : out   std_logic_vector(7 downto 0);

        DATA2     : inout std_logic_vector(7 downto 0);

        -- the following is 2-to-1 Multiplexer for connecting both MCUs to single RX pin (used for FW update)
        TXSEL1n2: in std_logic;         -- TX select -    1: TX_out <- TX_Franz,    0: TX_out <- TX_Hans
        TX_Franz: in  std_logic;        -- TX from Franz
        TX_Hans : in  std_logic;        -- TX from Hans
        TX_out  : out std_logic;        -- muxed TX

        clk     : in std_logic;

        -- used for real HW type identification
        HDD_IF  : in std_logic          -- 0 when ACSI, 1 when SCSI 
        ) ;
end main;

architecture Behavioral of main is
    signal REQtrig       : std_logic;
    signal REQstate      : std_logic;

    signal DATA1latch    : std_logic_vector(7 downto 0);
    signal resetCombo    : std_logic;
    signal identify      : std_logic;

    signal CDsignal      : std_logic;
    signal MSGsignal     : std_logic;
    signal IOsignal      : std_logic;

    signal oddParity     : std_logic;
    signal BSYsignal     : std_logic;

    signal d1out         : std_logic;
    
    signal identifyA     : std_logic;

    signal nSelection    : std_logic;
    signal lathClock     : std_logic;

    signal REQtrigD1     : std_logic;
    signal REQtrigD2     : std_logic;    

    signal SACKD1        : std_logic;
    signal SACKD2        : std_logic;    
    signal SACKD3        : std_logic;    
    signal SACKshort     : std_logic;    

    signal statusReg     : std_logic_vector(7 downto 0);
    signal softReset     : std_logic;

begin

    identify   <= XPIO and XDMA and TXSEL1n2;   -- when TXSEL1n2 selects Franz (='1') and you have PIO and DMA pins high, then you can read the identification byte from DATA2
    identifyA  <= identify and (not HDD_IF);    -- active when IDENTIFY and it's ACSI hardware
    softReset  <= identify and XRnW;            -- soft reset is done when it's IDENTIFY, but in READ direction (normally should be in WRITE direction)

    nSelection <= not ((not SEL) and BSYa);     -- selection is when SEL is 0 and BSY is 1. nSelection is inverted selection, because DATA1latch is captured on falling edge
    XCMD       <= nSelection or (not SRST);     -- falling edge means target selection (SRST must be 1, otherwise ignored)

    resetCombo <= SRST and reset_hans and (not softReset);   -- when one of these reset signals is low, the result is low

    REQtrig    <= XPIO xor XDMA xor XMSG;       -- REQ trig should go hi only when one of these (not 2, not 3) go hi

    captureAndDelay: process(clk, REQtrig, SACK) is  -- delaying process
    begin
        if(rising_edge(clk)) then
            REQtrigD2 <= REQtrigD1;             -- previous state of REQtrig
            REQtrigD1 <= REQtrig;               -- current  state in REQtrig

            SACKD3    <= SACKD2;                -- previous previous state of SACK
            SACKD2    <= SACKD1;                -- previous state of SACK
            SACKD1    <= SACK;                  -- current  state of SACK
        end if;
    end process;

	SACKshort    <= '0' when (SACKD3    = '1' and SACKD2    = '0') else '1';

    REQstateDflipflop: process(resetCombo, SACKshort, REQtrigD2) is
    begin
		if   (resetCombo = '0' or SACKshort = '0')  then	-- if phase reset or ACK is low, REQ state back to hi
            REQstate <= '1';
		elsif(rising_edge(REQtrigD2)) then
	        REQstate <= '0';
		end if;
    end process;

    busStateSignals: process(REQtrig, resetCombo) is
    begin
        if (resetCombo = '0') then	                    -- if phase reset
            BSYsignal   <= '1';                         -- not BSY
            IOsignal    <= '1';                         -- release I/O
            CDsignal    <= '1';                         -- release C/D
            MSGsignal   <= '1';                         -- release MSG
        elsif (rising_edge(REQtrig)) then               -- if REQ trig goes hi
            BSYsignal   <= '0';                         -- we're BSY
            IOsignal    <= not XRnW;                    -- set I/O - low on IN, hi on OUT
            CDsignal    <= XDMA;                        -- set C/D - low on CMD and MSG, hi on DATA
            MSGsignal   <= not XMSG;                    -- set MSG - low on MSG IN/OUT, otherwise hi
        end if;
    end process;

    SREQa <= '0' when (REQstate = '0' and SACKD2 = '1') else 'Z';          -- REQ - pull to L, otherwise hi-Z
    SREQb <= '0' when (REQstate = '0' and SACKD2 = '1') else 'Z';          -- REQ - pull to L, otherwise hi-Z

    -- 8-bit latch register
    -- latch data from ST on falling edge of lathClock (that means either on falling nSelection, or falling SACK)
    lathClock <= SACK and nSelection;

    dataLatch: process(lathClock) is
    begin 
        if (falling_edge(lathClock)) then
            DATA1latch <= not DATA1a;                    -- store inverted value to latch - SCSI SE has inverted data
        end if;
    end process;

    -- d1out says whether should output DATA1 -- only when XRnW is 1 (read) and resetCombo is 1 (not in reset state), otherwise don't drive DATA1 pins
    d1out <= '1' when ((XRnW='1' ) and (resetCombo='1')) else '0';  

    -- DATA1a and DATA1b are connected to SCSI bus, data goes out when going from MCU to ST (READ operation) and it's not reset state (this means d1out is 1)
    -- Also output inverted version of DATA2, because SCSI SE has inverted data
    DATA1a(7) <= '0' when ((d1out='1') and (DATA2(7)='1')) else 'Z';
    DATA1b(7) <= '0' when ((d1out='1') and (DATA2(7)='1')) else 'Z';
    DATA1a(6) <= '0' when ((d1out='1') and (DATA2(6)='1')) else 'Z';
    DATA1b(6) <= '0' when ((d1out='1') and (DATA2(6)='1')) else 'Z';
    DATA1a(5) <= '0' when ((d1out='1') and (DATA2(5)='1')) else 'Z';
    DATA1b(5) <= '0' when ((d1out='1') and (DATA2(5)='1')) else 'Z';
    DATA1a(4) <= '0' when ((d1out='1') and (DATA2(4)='1')) else 'Z';
    DATA1b(4) <= '0' when ((d1out='1') and (DATA2(4)='1')) else 'Z';
    DATA1a(3) <= '0' when ((d1out='1') and (DATA2(3)='1')) else 'Z';
    DATA1b(3) <= '0' when ((d1out='1') and (DATA2(3)='1')) else 'Z';
    DATA1a(2) <= '0' when ((d1out='1') and (DATA2(2)='1')) else 'Z';
    DATA1b(2) <= '0' when ((d1out='1') and (DATA2(2)='1')) else 'Z';
    DATA1a(1) <= '0' when ((d1out='1') and (DATA2(1)='1')) else 'Z';
    DATA1b(1) <= '0' when ((d1out='1') and (DATA2(1)='1')) else 'Z';
    DATA1a(0) <= '0' when ((d1out='1') and (DATA2(0)='1')) else 'Z';
    DATA1b(0) <= '0' when ((d1out='1') and (DATA2(0)='1')) else 'Z';

    -- create status register, which consists of fixed values (HW ver 2, SCSI Xilinx FW), and current values - HW/FW mismatch, BSY low or high
    statusReg(7) <= BSYsignal;      -- BSY signal - 0 means we're still busy!
    statusReg(6) <= '0';
    statusReg(5) <= '1';            -- - 10 means HW v.2
    statusReg(4) <= '0';            -- / 
    statusReg(3) <= identifyA;      -- when this is '1' -- BAD : when identify condition met, this identifies the XILINX and HW revision (0010 - HW rev 2, 1 - it's ACSI HW, 010 - SCSI Xilinx FW)
    statusReg(2) <= '0';            -- \
    statusReg(1) <= '1';            -- --- 010 = SCSI Xilinx FW
    statusReg(0) <= '0';            -- / 

    -- DATA2 is connected to Hans (STM32 mcu), data goes out when going from ST to MCU (WRITE operation)
    DATA2 <=    "ZZZZZ0ZZ"  when TXSEL1n2='0'   else   -- when TXSEL1n2 selects Hans, we're writing to Hans's flash, we need bit DATA2.2 (bit #2) to be 0 (it's BOOT1 bit on STM32 MCU)
                statusReg   when identify='1'   else   -- when doing IDENTIFY, return STATUS REGISTER - BSY signal value, HW version, FW type (ACSI or SCSI), HW / FW mismatch bit
                DATA1latch  when XRnW='0'       else   -- when set in WRITE direction, output latched DATA1 to DATA2 
                "ZZZZZZZZ";                             -- otherwise don't drive this

    -- TX_out is connected to RPi, and this is multiplexed TX pin from two MCUs
    TX_out <=   TX_Franz when TXSEL1n2='1' else TX_Hans;   -- depending on TXSEL1n2 switch TX_out to TX_Franz or TX_Hans

    -- just copy state from one signal to another
    XCS    <= SACKshort or (    CDsignal);
    XACK   <= SACKshort or (not CDsignal);
    XRESET <= SRST;

    -- these should be set according to the current SCSI phase
    CDa   <= '0' when CDsignal ='0' else 'Z';
    CDb   <= '0' when CDsignal ='0' else 'Z';

    SMSGa <= '0' when MSGsignal='0' else 'Z';
    SMSGb <= '0' when MSGsignal='0' else 'Z';

    IOa   <= '0' when IOsignal ='0' else 'Z';
    IOb   <= '0' when IOsignal ='0' else 'Z';

    -- odd parity output for IN direction
    oddParity <= ( (DATA2(0) xor DATA2(1)) xor (DATA2(2) xor DATA2(3)) ) xor ( (DATA2(4) xor DATA2(5)) xor (DATA2(6) xor DATA2(7)) );
    DBPa      <= '0' when ((IOsignal='0') and (oddParity='0')) else 'Z';        -- when in *_IN phase and parity is 0, then set it to 0; otherwise let it in 'Z'
    DBPb      <= '0' when ((IOsignal='0') and (oddParity='0')) else 'Z';        -- when in *_IN phase and parity is 0, then set it to 0; otherwise let it in 'Z'

    -- pull BSY low when needed, otherwise just let it in Hi-Z
    -- setting BSY signal depending on BUS STATE
    BSYa      <= '0' when BSYsignal='0' else 'Z';
    BSYb      <= '0' when BSYsignal='0' else 'Z';

end Behavioral;
