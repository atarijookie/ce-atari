library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

-- commands which trigger REQ
-- XPIO XRnW XDMA
--    0    0    ^    - PIO transfer - CMD          "00^"
--    0    1    ^    - PIO transfer - STATUS       "01^"
--    1    0    ^    - DMA transfer - DMA OUT      "10^"
--    1    1    ^    - DMA transfer - DMA IN       "11^"

-- commands which DON'T trigger REQ
-- XPIO  XRnW XDMA
--    1    0    1    - Identify (write)             "101"
--    1    1    1    - RESET    (read)              "111"


-- DMA OUT vs Identify:
-- set XPIO to 1
-- set XRnW to 0
-- set XDMA to 1, this rising_edge(XDMA) will cause to start the DRQ + ACK DMA sequence - this is start of DMA 
-- while XPIO and XDMA is 1, you can read status byte - this is Identify
-- set XDMA to 0, you can read the DATA1latch if XRnW = 0
-- thus reading status byte will always result in one DMA phase, but can be reset 

-- DMA IN vs Reset:
-- set XPIO to 1 
-- set XRnW to 1
-- set XDMA to 1, this rising_edge(XDMA) will cause to start the DRQ + ACK DMA sequence - this is start of DMA
-- while XPIO, XDMA and XRnW is 1, the softReset is 1, and thus will terminate the above DRQ + ACK sequence - should softReset be removed from ACSI? 

entity main is
    Port ( 
        -- signals connected to MCU
           XPIO        : in std_logic;         -- on rising edge will put INT to L
           XDMA        : in std_logic;         -- on rising edge will put DRQ to L
           XRnW        : in std_logic;         -- defines data direction (1: DATA1 <- DATA2,  0: DATA1 -> DATA2)
           XCMD        : out std_logic;        -- this is combination of CS and A1, will go low on 1st cmd byte from ACSI port
           reset_hans  : in std_logic;         -- this is the signal which resets Hans, and it will reset this CPLD too to init it

        -- signals connected to MCU, which should be just copies of ACSI port states
           XRESET  : out std_logic;
           XCS     : out std_logic;
           XACK    : out std_logic;

        -- signals connected to ACSI port
           AINTa   : out std_logic;
           AINTb   : out std_logic;

           ADRQa   : out std_logic;
           ADRQb   : out std_logic;

           ACSa    : in std_logic;
           ACSb    : in std_logic;
           
           AA1a    : in std_logic;
           AA1b    : in std_logic;
           
           AACKa   : in std_logic;
           AACKb   : in std_logic;

           ARESETa : in std_logic;
           ARESETb : in std_logic;

           ARNWa   : in std_logic;
           ARNWb   : in std_logic;

        -- DATA1 is connected to ACSI port, DATA2 is data latched on CS and ACK and connected to MCU
           DATA1a  : inout std_logic_vector(7 downto 0);
           DATA1b  :   out std_logic_vector(7 downto 0);
           
           DATA2   : inout std_logic_vector(7 downto 0);

        -- the following is 2-to-1 Multiplexer for connecting both MCUs to single RX pin (used for FW update)
           TXSEL1n2: in std_logic;         -- TX select -    1: TX_out <- TX_Franz,    0: TX_out <- TX_Hans
           TX_Franz: in  std_logic;        -- TX from Franz
           TX_Hans : in  std_logic;        -- TX from Hans
           TX_out  : out std_logic;        -- muxed TX

        -- used for real HW type identification
           HDD_IF  : in std_logic          -- 0 when ACSI, 1 when SCSI 
        ) ;
end main;

architecture Behavioral of main is
    signal INTstate  : std_logic;
    signal DRQstate  : std_logic;
    signal DATA1latch: std_logic_vector(7 downto 0);
    signal latchClock: std_logic;
    signal resetCombo: std_logic;
    signal identify  : std_logic;
    
    signal ACS       : std_logic;
    signal AA1       : std_logic;
    signal AACK      : std_logic;
    signal ARESET    : std_logic;
    signal ARNW      : std_logic;

    signal DATA1rnw  : std_logic;
    
    signal statusReg : std_logic_vector(7 downto 0);
    signal softReset : std_logic;
    
begin
    identify   <= XPIO and XDMA and (not XRnW) and TXSEL1n2;    -- when TXSEL1n2 selects Franz (='1') and you have PIO and DMA pins high, then you can read the identification byte from DATA2
    softReset  <= XPIO and XDMA and (    XRnW);                 -- soft reset is done when it's IDENTIFY, but in READ direction (normally should be in WRITE direction)
    resetCombo <= ARESET and reset_hans and (not softReset);    -- when one of these reset signals is low, the result is low
      
    XCMD <= ACS or AA1 or (not ARESET);                         -- CMD - falling edge here will tell that CS with A1 low has been found (and ACSI RESET has to be high at that time)
           
    -- these are here to create single signal from double input pins, which should have the same value
    ACS     <= ACSa    and ACSb;
    AA1     <= AA1a    and AA1b;
    AACK    <= AACKa   and AACKb;
    ARESET  <= ARESETa and ARESETb; 
    ARNW    <= ARNWa   and ARNWb;

    latchClock <= ACS and AACK;                         -- need this only to be able to react on falling edge of both CS and ACK
           
    -- D flip-flop with asynchronous reset 
    -- pull INT low after rising edge of PIO, let it in hi-Z after reset or low on CS
    -- DMA pin has to be low when toggling PIO hi and low
    PIOrequest: process(XPIO, XDMA, latchClock, resetCombo) is
    begin
        if (latchClock = '0' or resetCombo = '0') then  -- if reset condition, reset these signals
            INTstate <= '1';
            DRQstate <= '1';
        elsif (rising_edge(XDMA)) then                  -- rising edge of XDMA
            if (XPIO = '0') then                        -- when XPIO is L, it's PIO transfer
                INTstate <= '0';
            else                                        -- when XPIO is H, it's DMA transfer
                DRQstate <= '0';
            end if;
        end if;
    end process;

    -- 8-bit latch register
    -- latch data from ST on falling edge of latchClock, which is CS and ACK
    dataLatch: process(latchClock) is
    begin 
        if (falling_edge(latchClock)) then
            DATA1latch <= DATA1a;
        end if;
    end process;

    AINTa <= '0' when INTstate='0' else 'Z';            -- INT - pull to L, otherwise hi-Z
    AINTb <= '0' when INTstate='0' else 'Z';            -- INT - pull to L, otherwise hi-Z

    ADRQa <= '0' when DRQstate='0' else 'Z';            -- DRQ - pull to L, otherwise hi-Z
    ADRQb <= '0' when DRQstate='0' else 'Z';            -- DRQ - pull to L, otherwise hi-Z

    DATA1rnw <= XRnW and ARNW;                          -- output data only when XILINX and ST want to read data, otherwise don't

    -- create status register, which consists of fixed values (HW ver 2, SCSI Xilinx FW), and current values - HW/FW mismatch, BSY low or high
    statusReg(7) <= DRQstate and INTstate;  -- if one of these is 0, then resulting 0 means we're still busy!
    statusReg(6) <= '0';
    statusReg(5) <= '1';            -- - 10 means HW v.2
    statusReg(4) <= '0';            -- / 
    statusReg(3) <= HDD_IF;         -- when this is '1' -- BAD : when identify condition met, this identifies the XILINX and HW revision (0010 - HW rev 2, 1 - it's ACSI HW, 001 - ACSI Xilinx FW)
    statusReg(2) <= '0';            -- \
    statusReg(1) <= '0';            -- --- 001 = ACSI Xilinx FW
    statusReg(0) <= '1';            -- / 
    
    -- DATA1 is connected to Atari ST, data goes out when going from MCU to ST (READ operation)
    DATA1a <=   "ZZZZZZZZ"  when resetCombo='0' else    -- when Atari or MCU is in reset state, don't drive this 
                DATA2       when DATA1rnw='1'   else    -- when set in READ direction, transparently bridge data from DATA2 to DATA1
                "ZZZZZZZZ";                             -- otherwise don't drive this

    DATA1b <=   "ZZZZZZZZ"  when resetCombo='0' else    -- when Atari or MCU is in reset state, don't drive this 
                DATA2       when DATA1rnw='1'   else    -- when set in READ direction, transparently bridge data from DATA2 to DATA1
                "ZZZZZZZZ";                             -- otherwise don't drive this

    -- DATA2 is connected to Hans (STM32 mcu), data goes out when going from ST to MCU (WRITE operation)
    DATA2 <=    "ZZZZZ0ZZ"  when TXSEL1n2='0'   else   -- when TXSEL1n2 selects Hans, we're writing to Hans's flash, we need bit DATA2.2 (bit #2) to be 0 (it's BOOT1 bit on STM32 MCU)
                statusReg   when identify='1'   else   -- when doing IDENTIFY, return STATUS REGISTER - BSY signal value, HW version, FW type (ACSI or SCSI), HW / FW mismatch bit
                DATA1latch  when XRnW='0'       else   -- when set in WRITE direction, output latched DATA1 to DATA2 
                "ZZZZZZZZ";                            -- otherwise don't drive this

    -- TX_out is connected to RPi, and this is multiplexed TX pin from two MCUs
    TX_out <=   TX_Franz when TXSEL1n2='1' else TX_Hans;   -- depending on TXSEL1n2 switch TX_out to TX_Franz or TX_Hans

    -- just copy state from one signal to another
    XRESET <= ARESET;
    XCS    <= ACS;
    XACK   <= AACK;
    
end Behavioral;
