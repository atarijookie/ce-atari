library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

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
           AINT    : out std_logic;
           ADRQ    : out std_logic;
           ACS     : in std_logic;
           AA1     : in std_logic;
           AACK    : in std_logic;
           ARESET  : in std_logic;

        -- DATA1 is connected to ACSI port, DATA2 is data latched on CS and ACK and connected to MCU
           DATA1   : inout std_logic_vector(7 downto 0);
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
    signal identifyA : std_logic;
    signal identifyS : std_logic;

begin
           
    -- D flip-flop with asynchronous reset 
    -- pull INT low after rising edge of PIO, let it in hi-Z after reset or low on CS
    -- DMA pin has to be low when toggling PIO hi and low
    PIOrequest: process(XPIO, XDMA, latchClock, ARESET, reset_hans) is
    begin
        if ((latchClock = '0') or (ARESET = '0') or (reset_hans = '0')) then
            INTstate <= '1';
        elsif (rising_edge(XPIO) and (XDMA = '0')) then
            INTstate <= '0';
        end if;
    end process;

    -- D flip-flop with asynchronous reset 
    -- pull DRQ low after rising edge of DMA, let it in hi-Z after reset or low on ACK
    -- PIO pin has to be low when toggling DMA hi and low
    DMArequest: process(XDMA, XPIO, latchClock, ARESET, reset_hans) is
    begin
        if ((latchClock = '0') or (ARESET = '0') or (reset_hans = '0')) then
            DRQstate <= '1';
        elsif (rising_edge(XDMA) and (XPIO = '0')) then
            DRQstate <= '0';
        end if;
    end process;

    -- 8-bit latch register
    -- latch data from ST on falling edge of latchClock, which is CS and ACK
    dataLatch: process(latchClock) is
    begin 
        if (falling_edge(latchClock)) then
            DATA1latch <= DATA1;
        end if;
    end process;

    latchClock <= ACS and AACK;                         -- need this only to be able to react on falling edge of both CS and ACK
    resetCombo <= ARESET and reset_hans;                -- when at least one of those 2 reset signals is low, the result is low

    identify   <= XPIO and XDMA and TXSEL1n2;           -- when TXSEL1n2 selects Franz (='1') and you have PIO and DMA pins high, then you can read the identification byte from DATA2
    identifyA  <= identify and (not HDD_IF);            -- active when IDENTIFY and it's ACSI hardware
    identifyS  <= identify and HDD_IF;                  -- active when IDENTIFY and it's SCSI hardware

    AINT <= '0' when INTstate='0' else 'Z';             -- INT - pull to L, otherwise hi-Z
    ADRQ <= '0' when DRQstate='0' else 'Z';             -- DRQ - pull to L, otherwise hi-Z
    XCMD <= ACS or AA1 or (not ARESET);                 -- CMD - falling edge here will tell that CS with A1 low has been found (and ACSI RESET has to be high at that time)

    -- DATA1 is connected to Atari ST, data goes out when going from MCU to ST (READ operation)
    DATA1 <=    "ZZZZZZZZ"  when resetCombo='0' else    -- when Atari or MCU is in reset state, don't drive this 
                DATA2       when XRnW='1'       else    -- when set in READ direction, transparently bridge data from DATA2 to DATA1
                "ZZZZZZZZ";                             -- otherwise don't drive this

    -- DATA2 is connected to Hans (STM32 mcu), data goes out when going from ST to MCU (WRITE operation)
    DATA2 <=    "ZZZZZ0ZZ"  when TXSEL1n2='0'    else   -- when TXSEL1n2 selects Hans, we're writing to Hans's flash, we need bit DATA2.2 (bit #2) to be 0 (it's BOOT1 bit on STM32 MCU)
                "00100001"  when identifyA='1'   else   -- GOOD: when identify condition met, this identifies the XILINX and HW revision (0010 - HW rev 2, 0 - it's ACSI HW, 001 - ACSI XILINX FW)
                "00101001"  when identifyS='1'   else   -- BAD : when identify condition met, this identifies the XILINX and HW revision (0010 - HW rev 2, 1 - it's SCSI HW, 001 - ACSI XILINX FW)
                DATA1latch  when XRnW='0'        else   -- when set in WRITE direction, output latched DATA1 to DATA2 
                "ZZZZZZZZ";                             -- otherwise don't drive this

    -- TX_out is connected to RPi, and this is multiplexed TX pin from two MCUs
    TX_out <=   TX_Franz when TXSEL1n2='1' else TX_Hans;   -- depending on TXSEL1n2 switch TX_out to TX_Franz or TX_Hans

    -- just copy state from one signal to another
    XRESET <= ARESET;
    XCS    <= ACS;
    XACK   <= AACK;

end Behavioral;
