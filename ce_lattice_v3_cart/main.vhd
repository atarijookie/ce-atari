library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

library machxo2;
use machxo2.all;

entity main is
    Port (
        -- signals connected to RPI
		XATN		: out std_logic;	-- goes H on 1st byte or when there are other bytes to transfer
		XRnW		: in std_logic;		-- defines data direction (1 (READ): DATA1 <- DATA2 (from RPi to ST),  0 (WRITE): DATA1 -> DATA2 (from ST to RPi))
		XDnC		: in std_logic;		-- Data/Config selection: 1 means data register is selected, 0 means config register is selected
		XNEXT		: in std_logic;		-- RPi tells ST that it wants more data
		XRESET		: in std_logic;		-- RPi sets CPLD to idle state, ST will know that no more data should be transfered

		-- DATA_ST_LOWER and DATA_ST_UPPER is connected to cartridge port, DATA_RPI connects to RPI and is driven when XRnW is L
		DATA_ST_UPPER: out   std_logic_vector(7 downto 0);
		DATA_ST_LOWER: out   std_logic_vector(7 downto 0);
		ENABLE_LOW : out std_logic;		-- enable DATA_ST_LOWER signal for external bus driver
		ENABLE_HIGH: out std_logic;		-- enable DATA_ST_UPPER signal for external bus driver
		
		DATA_RPI     : inout std_logic_vector(7 downto 0);

		-- ADDR are the address lines from cartridge port
		ADDR    : in std_logic_vector(9 downto 1);

		-- cartridge port signals for accessing the right areas
		LDS     : in std_logic;
		UDS     : in std_logic;
		ROM3    : in std_logic;			-- at ST address 0xFB0000 -- for CE transfers
		ROM4    : in std_logic;			-- at ST address 0xFA0000 -- for driver booting
		
		-- other
		TP1, TP2, TP3: out std_logic	-- either test point signals, or future (development) usage
        );
end main;

architecture Behavioral of main is
	signal clk				: std_logic;
	signal stdby_sed		: std_logic;

    signal RPIisIdle        : std_logic;
    signal RPIwantsMoreState: std_logic;
    signal statusReg        : std_logic_vector(7 downto 0);

    signal XATNState		: std_logic;
	signal XNext1, XNext2, XNext3: std_logic;

	signal XRnW1, XRnW2: std_logic;
	signal XDnC1, XDnC2: std_logic;
	
    signal st_doing_read: std_logic;

    signal READ_CART_LDS, READ_CART_LDS1: std_logic;
    signal READ_CART_UDS : std_logic;
    signal BOOT_CART_LDS : std_logic;
    signal BOOT_CART_UDS : std_logic;

	signal ROM31, ROM32	: std_logic;
	signal ROM41, ROM42	: std_logic;
	signal LDS1, LDS2	: std_logic;
	signal UDS1, UDS2	: std_logic;
	
	signal data_read: std_logic;
	signal data_write: std_logic;
	signal rpi_strobe: std_logic;
	signal config_read_op: std_logic;
	signal config_write_op: std_logic;
	signal config_read: std_logic;
	signal config_write: std_logic;
	signal rpi_writing_to_fifo: std_logic;
	signal rpi_reading_from_fifo: std_logic;

	signal st_strobe: std_logic;
	signal st_writing_to_fifo: std_logic;
	signal st_reading_from_fifo: std_logic;

	COMPONENT OSCH
	   GENERIC(
			 NOM_FREQ: string := "16.63"); -- 16.63MHz
	   PORT(
			 STDBY    : IN  STD_LOGIC;     --'0' OSC output is active, '1' OSC output off
			 OSC      : OUT STD_LOGIC;     -- the oscillator output
			 SEDSTDBY : OUT STD_LOGIC);    -- required only for simulation when using standby
	END COMPONENT;
	
	-- FIFO_DC component declaration
	component fifo_dc
		port (Data: in  std_logic_vector(7 downto 0); WrClock: in  std_logic; 
			RdClock: in  std_logic; WrEn: in  std_logic; RdEn: in  std_logic; 
			Reset: in  std_logic; RPReset: in  std_logic; 
			Q: out  std_logic_vector(7 downto 0); Empty: out  std_logic; 
			Full: out  std_logic; AlmostEmpty: out  std_logic; 
			AlmostFull: out  std_logic);
	end component;

	signal fifo_data_in: std_logic_vector(7 downto 0); 
	signal fifo_data_out: std_logic_vector(7 downto 0); 
	signal fifo_WrEn: std_logic; 
	signal fifo_RdEn: std_logic; 
	signal fifo_Empty: std_logic; 
	signal fifo_Full: std_logic; 
	signal fifo_AlmostEmpty: std_logic; 
	signal fifo_AlmostFull: std_logic;

	signal fifo_byte_count: unsigned(10 downto 0) := "00000000000";	-- 1024 bytes in FIFO -- needs 11 bits to store number 1024
	signal fifo_byte_count8b: unsigned(7 downto 0);					-- count of bytes, rounded to 8 bits (e.g. when there are 256 and more bytes in fifo, this will return 255)
	signal fifo_byte_count7b: unsigned(6 downto 0);					-- count of bytes, rounded to 7 bits (e.g. when there are 128 and more bytes in fifo, this will return 127)

	-- config:
	--  0 - 0000: PIO write 1st - waiting for 1st cmd byte, XATN shows FIFO-not-empty (can read)
	--  1 - 0001: PIO write - data from ST to RPi, XATN shows FIFO-not-empty (can read)
	--  2 - 0010: DMA write - data from ST to RPi, XATN shows FIFO-not-empty (can read)
	--  3 - 0011: MSG write - data from ST to RPi, XATN shows FIFO-not-empty (can read)
	--  4 - 0100: PIO read  - data from RPi to ST, XATN shows FIFO-not-full  (can write)
	--  5 - 0101: DMA read  - data from RPi to ST, XATN shows FIFO-not-full  (can write)
	--  6 - 0110: MSG read  - data from RPi to ST, XATN shows FIFO-not-full  (can write)
	--  7 - 0111: read fifo_byte_count -- how many bytes are stored in FIFO and can be read out
	--
	-- 13 - 1101: SRAM_write - store another byte into boot SRAM (index register is reset to zero with reset)
	-- 14 - 1110: current IF - to determine, if it's ACSI, SCSI or CART (because SCSI needs extra MSG phases)
	-- 15 - 1111: current version - to determine, if this chip needs update or not
    signal configReg: std_logic_vector(3 downto 0) := "0000";

	-- ROM3 - CE transfers, ROM4 - driver boot from cart

begin
	-- OSCH Instantiation
	OSCInst: OSCH
	-- synthesis translate_off
		GENERIC MAP( NOM_FREQ => "16.63" )
	-- synthesis translate_on
		PORT MAP (STDBY=> '0', OSC => clk, SEDSTDBY => stdby_sed);
	
	-- FIFO_DC Instantiation
	FIFO: fifo_dc
		port map (Data(7 downto 0)=>fifo_data_in, WrClock=>clk, RdClock=>clk, WrEn=>fifo_WrEn, 
			RdEn=>fifo_RdEn, Reset=>XRESET, RPReset=>XRESET, Q(7 downto 0)=>fifo_data_out, Empty=>fifo_Empty, 
			Full=>fifo_Full, AlmostEmpty=>fifo_AlmostEmpty, AlmostFull=>fifo_AlmostFull);	
	
	InputSynchronizer: process(clk) is
    begin
        if rising_edge(clk) then
			-- RPi signals sync
			XNext1 <= XNEXT;		
			XNext2 <= XNext1;
			XNext3 <= XNext2;

			XRnW1 <= XRnW;
			XRnW2 <= XRnW1;

			XDnC1 <= XDnC;
			XDnC2 <= XDnC1;

			-- cart signals sync
			ROM31 <= ROM3;
			ROM32 <= ROM31;

			ROM41 <= ROM4;
			ROM42 <= ROM41;

			LDS1 <= LDS;
			LDS2 <= LDS1;

			UDS1 <= UDS;
			UDS2 <= UDS1;
			
			READ_CART_LDS <= ((not ROM32) and (not LDS2));  	-- H when LDS and ROM3 are L
			READ_CART_UDS <= ((not ROM32) and (not UDS2));  	-- H when UDS and ROM3 are L
			READ_CART_LDS1 <= READ_CART_LDS;					-- prev value

			ENABLE_LOW <= not READ_CART_LDS;					-- enable is active LOW - when READ_CART_LDS is active
			ENABLE_HIGH <= not READ_CART_UDS;					-- enable is active LOW - when READ_CART_UDS is active

			BOOT_CART_LDS <= ((not ROM42) and (not LDS2));  	-- H when LDS and ROM4 are L
			BOOT_CART_UDS <= ((not ROM42) and (not UDS2));  	-- H when UDS and ROM4 are L
		end if;
    end process;

	rpi_strobe      <= XNext2 and (not XNext3);			-- on rising edge of XNext
	data_read       <= XDnC2 and XRnW2;					-- on data read (read direction is from RPi to ST)
	data_write      <= XDnC2 and (not XRnW2);			-- on data write (write direction is from ST to RPi)
	config_write    <= (not XDnC2) and XRnW2;			-- on config write (read direction is from RPi to logic)
	config_read     <= (not XDnC2) and (not XRnW2);		-- on config read  (write direction is from logic to RPi)
	config_read_op  <= '1' when (configReg="0100" or configReg="0101" or configReg="0110") else '0';							-- on config set to PIO, DMA or MSG read
	config_write_op <= '1' when (configReg="0000" or configReg="0001" or configReg="0010" or configReg="0011") else '0';		-- on config set to PIO 1st, PIO other, DMA or MSG write
	RPIisIdle       <= '1' when configReg="0000" else '0';		-- when RPi is waiting for 1st cmd byte, it's idle

	rpi_writing_to_fifo   <= '1' when (data_read='1'  and config_read_op='1'  and rpi_strobe='1') else '0';
	rpi_reading_from_fifo <= '1' when (data_write='1' and config_write_op='1' and rpi_strobe='1') else '0';

	st_strobe            <= '1' when (READ_CART_LDS='1' and READ_CART_LDS1='0') else '0';		-- on rising edge of READ_CART_LDS
	st_writing_to_fifo   <= '1' when (config_write_op='1' and READ_CART_LDS='1') else '0';		-- when configured for WRITE and LDS is active
	st_reading_from_fifo <= '1' when (config_read_op='1'  and READ_CART_LDS='1') else '0';		-- when configured for READ  and LDS is active

	fifo_data_in <= DATA_RPI         when config_read_op ='1' else	-- when RPi is writing to FIFO (on READ  - from RPi to ST)
					ADDR(8 downto 1) when config_write_op='1' else	-- when ST  is writing to FIFO (on WRITE - from ST to RPi)
					"00000000";

	fifo_WrEn <= '1' when (rpi_writing_to_fifo='1') else				-- when RPi is writing to FIFO
				'1' when (config_write_op='1' and st_strobe='1') else	-- when ST  is writing to FIFO
				'0';

	fifo_RdEn <= '1' when (rpi_reading_from_fifo='1') else				-- when RPi is reading from FIFO
				'1' when (config_read_op='1' and st_strobe='1') else	-- when ST  is reading from FIFO
				'0';

	fifo_byte_count8b <= fifo_byte_count(7 downto 0) when (fifo_byte_count < 256) else "11111111";	-- if it's 255 or less, return that value, otherwise limit it to 255
	fifo_byte_count7b <= fifo_byte_count(6 downto 0) when (fifo_byte_count < 128) else  "1111111";	-- if it's 127 or less, return that value, otherwise limit it to 127

	DATA_RPI <= fifo_data_out when (data_write='1'  and config_write_op='1') else -- RPi can read FIFO data when configured data write and the operation in config is write operations
				std_logic_vector(fifo_byte_count8b) when (config_read='1' and configReg="0111") else	-- when reading fifo_byte_count, return this
				"00000011"    when (config_read='1' and configReg="1110") else    -- RPi can read IF type (1 is ACSI, 2 is SCSI, 3 is CART)
				"00000001"    when (config_read='1' and configReg="1111") else    -- RPi can read current version - to determine, if this chip needs update or not
				"ZZZZZZZZ";
				
    XATN <= (not fifo_Empty) when (config_write_op='1') else		-- on write operations - showing not-empty (can get data out of it)
			(not fifo_Full)  when (config_read_op='1') else 		-- on read  operations - showing not-full  (can store data into it)
			'0';

	FIFObyteCounter: process(clk, XRESET, fifo_WrEn, fifo_RdEn) is
    begin
		if XRESET='1' then						-- on reset - no bytes in FIFO
			fifo_byte_count <= "00000000000";
        elsif rising_edge(clk) then
			-- if both read and write happen at the same clock, don't change the value
			if    fifo_WrEn='1' and fifo_RdEn='0' then			-- on write: +1 byte in FIFO
				fifo_byte_count <= fifo_byte_count + 1;
			elsif fifo_WrEn='0' and fifo_RdEn='1' then			-- on read:  -1 byte in FIFO
				fifo_byte_count <= fifo_byte_count - 1;
			end if;
		end if;
	end process;

	RPiHandling: process(clk, rpi_strobe, config_write) is
    begin
		if XRESET='1' then									-- reset brings the config to 'PIO write 1st'
			configReg <= "0000";
		elsif rpi_strobe='1' and config_write='1' then		-- when doing config write and got rpi strobe, store new config
			configReg <= DATA_RPI(3 downto 0);
		end if;
    end process;

    -- DATA_ST_LOWER is connected to ST DATA(7 downto 0)
    DATA_ST_LOWER <= fifo_data_out when READ_CART_LDS='1' else 	-- give ST the FIFO data 
					"00000000" when BOOT_CART_LDS='1' else 		-- give ST the boot data
					"ZZZZZZZZ";

    -- status register for ST
    statusReg(7) <= RPIisIdle;          							-- when H, RPi doesn't do any further transfer (last byte was status byte)
	statusReg(6 downto 0) <= std_logic_vector(fifo_byte_count7b);	-- count of bytes we can transfer (0-127)

    -- DATA_ST_UPPER is connected to ST DATA(15 downto 8)
    DATA_ST_UPPER <= statusReg when READ_CART_UDS='1' else	-- upper data - status byte
					"00000000" when BOOT_CART_UDS='1' else	-- upper data - boot data
					"ZZZZZZZZ";

end Behavioral;
