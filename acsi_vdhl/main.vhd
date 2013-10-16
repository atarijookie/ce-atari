library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity main is
    Port ( 
    		-- signals connected to MCU
    		 PIO   : in std_logic;			-- on rising edge will put INT to L
           DMA   : in std_logic;			-- on rising edge will put DRQ to L
           RnW   : in std_logic;			-- defines data direction (1: DATA1 <- DATA2,  0: DATA1 -> DATA2)
           CMD   : out std_logic;			-- this is combination of CS and A1, will go low on 1st cmd byte from ACSI port

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
		 TXSEL1n2: in std_logic;			-- TX select -    1: TX_out <- TX1_in,    0: TX_out <- TX2_in 
		 TX1_in  : in  std_logic;		-- TX from first  MCU
		 TX2_in  : in  std_logic;		-- TX from second MCU
		 TX_out  : out std_logic			-- muxed TX
		) ;
end main;

architecture Behavioral of main is
	signal INTstate  : std_logic;
	signal DRQstate  : std_logic;
	signal DATA1latch: std_logic_vector(7 downto 0);
	signal latchClock: std_logic;

begin
		   
	-- D flip-flop with asynchronous reset 
	-- pull INT low after rising edge of PIO, let it in hi-Z after reset or low on CS
	PIOrequest: process(PIO, latchClock, RESET) is				
	begin
		if ((latchClock = '0') or (RESET = '0')) then
			INTstate <= '1';
		elsif (rising_edge(PIO)) then
			INTstate <= '0';
		end if;
	end process;

	-- D flip-flop with asynchronous reset 
	-- pull DRQ low after rising edge of DMA, let it in hi-Z after reset or low on ACK
	DMArequest: process(DMA, latchClock, RESET) is				
	begin
		if ((latchClock = '0') or (RESET = '0')) then
			DRQstate <= '1';
		elsif (rising_edge(DMA)) then
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

	latchClock <= CS and ACK;						-- need this only to be able to react on falling edge of both CS and ACK

	INT <= '0' when INTstate='0' else 'Z';				-- INT - pull to L, otherwise hi-Z
	DRQ <= '0' when DRQstate='0' else 'Z';				-- DRQ - pull to L, otherwise hi-Z
	CMD <= CS or A1; 								-- CMD - falling edge here will tell that CS with A1 low has been found

	DATA1 <= DATA2      when RnW='1' else "ZZZZZZZZ";		-- from MCU to ST  on READ
	DATA2 <= DATA1latch when RnW='0' else "ZZZZZZZZ";		-- from ST  to MCU on WRITE

	TX_out <=	TX1_in when TXSEL1n2='1' else TX2_in;		-- depending on TXSEL1n2 switch TX_out to TX1 or TX2

end Behavioral;
