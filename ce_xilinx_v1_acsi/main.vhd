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
            PIO         : in std_logic;         -- on rising edge will put INT to L
            DMA         : in std_logic;         -- on rising edge will put DRQ to L
            RnW         : in std_logic;         -- defines data direction (1: DATA1 <- DATA2,  0: DATA1 -> DATA2)
            CMD         : out std_logic;        -- this is combination of CS and A1, will go low on 1st cmd byte from ACSI port
            reset_hans  : in std_logic;         -- this is the signal which resets Hans, and it will reset this CPLD too to init it

        -- signals connected to ACSI port
            INT   : out std_logic;
            DRQ   : out std_logic;
            CS    : in std_logic;
            A1    : in std_logic;
            ACK   : in std_logic;
            RESET : in std_logic;

        -- DATA1 is connected to ACSI port, DATA2 is data latched on CS and ACK and connected to MCU
           DATA1 : inout std_logic_vector(7 downto 0);
           DATA2 : inout std_logic_vector(7 downto 0);


        -- the following is 2-to-1 Multiplexer for connecting both MCUs to single RX pin (used for FW update)
            TXSELFnH: in std_logic;         -- TX select -    1: TX_out <- TX_Franz,    0: TX_out <- TX_Hanz
            TX_Franz: in  std_logic;        -- TX from first  MCU (Franz)
            TX_Hans : in  std_logic;        -- TX from second MCU (Hans)
            TX_out  : out std_logic         -- muxed TX
        ) ;
end main;

architecture Behavioral of main is
    signal INTstate  : std_logic;
    signal DRQstate  : std_logic;
    signal DATA1latch: std_logic_vector(7 downto 0);
    signal latchClock: std_logic;
    signal resetCombo: std_logic;
    signal identify  : std_logic;

    signal statusReg : std_logic_vector(7 downto 0);
    signal softReset : std_logic;
    
begin
    identify   <= PIO and DMA and (not RnW) and TXSELFnH;   -- when TXSELFnH selects Franz (='1') and you have PIO and DMA pins high, then you can read the identification byte from DATA2
    softReset  <= PIO and DMA and (    RnW);                -- soft reset is done when it's IDENTIFY, but in READ direction (normally should be in WRITE direction)
    resetCombo <= RESET and reset_hans and (not softReset); -- when one of these reset signals is low, the result is low
           
    CMD         <= CS or A1 or (not RESET);                 -- CMD - falling edge here will tell that CS with A1 low has been found (and ACSI RESET has to be high at that time)
    latchClock  <= CS and ACK;                              -- need this only to be able to react on falling edge of both CS and ACK

    -- D flip-flop with asynchronous reset 
    -- pull INT low after rising edge of PIO, let it in hi-Z after reset or low on CS
    -- DMA pin has to be low when toggling PIO hi and low
    PIOrequest: process(PIO, DMA, latchClock, resetCombo) is
    begin
        if (latchClock = '0' or resetCombo = '0') then  -- if reset condition, reset these signals
            INTstate <= '1';
            DRQstate <= '1';
        elsif (rising_edge(DMA)) then                   -- rising edge of DMA
            if (PIO = '0') then                         -- when PIO is L, it's PIO transfer
                INTstate <= '0';
            else                                        -- when PIO is H, it's DMA transfer
                DRQstate <= '0';
            end if;
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

    INT <= '0' when INTstate='0' else 'Z';              -- INT - pull to L, otherwise hi-Z
    DRQ <= '0' when DRQstate='0' else 'Z';              -- DRQ - pull to L, otherwise hi-Z

    -- create status register, which consists of fixed values (HW ver 1, ACSI Xilinx FW), and current values - BSY low or high
    statusReg(7) <= DRQstate and INTstate;  -- if one of these is 0, then resulting 0 means we're still busy!
    statusReg(6) <= '0';
    statusReg(5) <= '0';            -- - 01 means HW v.1
    statusReg(4) <= '1';            -- / 
    statusReg(3) <= '0';            -- never a HW / FW mismatch - HW ver 1 is always ACSI
    statusReg(2) <= '0';            -- \
    statusReg(1) <= '0';            -- --- 001 = ACSI Xilinx FW
    statusReg(0) <= '1';            -- / 
    
    -- DATA1 is connected to Atari ST, data goes out when going from MCU to ST (READ operation)
    DATA1 <=    "ZZZZZZZZ"  when resetCombo='0' else    -- when Atari or MCU is in reset state, don't drive this 
                DATA2       when RnW='1'        else    -- when set in READ direction, transparently bridge data from DATA2 to DATA1
                "ZZZZZZZZ";                             -- otherwise don't drive this

    -- DATA2 is connected to Hans (STM32 mcu), data goes out when going from ST to MCU (WRITE operation)
    DATA2 <=    "ZZZZZ0ZZ"  when TXSELFnH='0'    else   -- when TXSELFnH selects Hans, we're writing to Hans's flash, we need bit DATA2.2 (bit #2) to be 0 (it's BOOT1 bit on STM32 MCU)
                statusReg   when identify='1'    else   -- when identify condition met, this identifies the XILINX and HW revision (0001 - HW rev 1, 0001 - ACSI version)
                DATA1latch  when RnW='0'         else   -- when set in WRITE direction, output latched DATA1 to DATA2 
                "ZZZZZZZZ";                             -- otherwise don't drive this

    -- TX_out is connected to RPi, and this is multiplexed TX pin from two MCUs
    TX_out <=   TX_Franz when TXSELFnH='1' else TX_Hans; -- depending on TXSELFnH switch TX_out to TX_Franz or TX_Hans

end Behavioral;
