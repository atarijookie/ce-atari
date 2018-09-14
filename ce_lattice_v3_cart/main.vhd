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
		DATA_ST_UPPER: out   std_logic_vector(2 downto 0);
		DATA_ST_LOWER: out   std_logic_vector(7 downto 0);
		DATA_RPI     : inout std_logic_vector(7 downto 0);

		-- ADDR are the address lines from cartridge port
		ADDR    : in std_logic_vector(8 downto 1);

		-- cartridge port signals for accessing the right areas
		LDS     : in std_logic;
		UDS     : in std_logic;
		ROM3    : in std_logic
--		ROM4    : in std_logic
        );
end main;

architecture Behavioral of main is
	signal clk				: std_logic;
	signal stdby_sed		: std_logic;

    signal RPIisIdle        : std_logic;
    signal RPIwantsMoreState: std_logic;
    signal DataChangedState : std_logic;     -- toggles every time RPi changes data (on XNEXT)
    signal statusReg        : std_logic_vector(2 downto 0);

    signal XATNState		: std_logic;
	signal XNext1, XNext2, XNext3: std_logic;

	signal XRnW1, XRnW2: std_logic;
	signal XDnC1, XDnC2: std_logic;
	
    signal st_data_latched: std_logic_vector(7 downto 0);
    signal st_latch_clock: std_logic;
    signal st_doing_read: std_logic;

    signal READ_CART_LDS : std_logic;
    signal READ_CART_UDS : std_logic;
	
	signal ROM31, ROM32	: std_logic;
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

	-- config:
	--  0 - 0000: PIO write 1st - waiting for 1st cmd byte, XATN shows FIFO-not-empty (can read)
	--  1 - 0001: PIO write - data from ST to RPi, XATN shows FIFO-not-empty (can read)
	--  2 - 0010: DMA write - data from ST to RPi, XATN shows FIFO-not-empty (can read)
	--  3 - 0011: MSG write - data from ST to RPi, XATN shows FIFO-not-empty (can read)
	--  4 - 0100: PIO read  - data from RPi to ST, XATN shows FIFO-not-full  (can write)
	--  5 - 0101: DMA read  - data from RPi to ST, XATN shows FIFO-not-full  (can write)
	--  6 - 0110: MSG read  - data from RPi to ST, XATN shows FIFO-not-full  (can write)
	--  7 - 0111: SRAM_write - store another byte into boot SRAM (index register is reset to zero with reset)
	--  8 - 1000: current IF - to determine, if it's ACSI, SCSI or CART (because SCSI needs extra MSG phases)
	--  9 - 1001: current version - to determine, if this chip needs update or not
    signal configReg: std_logic_vector(3 downto 0);

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
		
			LDS1 <= LDS;
			LDS2 <= LDS1;

			UDS1 <= UDS;
			UDS2 <= UDS1;
		end if;
    end process;

    READ_CART_LDS <= ((not ROM32) and (not LDS2));  	-- H when LDS and ROM3 are L
    READ_CART_UDS <= ((not ROM32) and (not UDS2));  	-- H when UDS and ROM3 are L

	rpi_strobe     <= XNext2 and (not XNext3);			-- on rising edge of XNext
	data_read      <= XDnC2 and XRnW2;					-- on data read (read direction is from RPi to ST)
	data_write     <= XDnC2 and (not XRnW2);			-- on data write (write direction is from ST to RPi)
	config_write   <= (not XDnC2) and XRnW2;			-- on config write (read direction is from RPi to logic)
	config_read    <= (not XDnC2) and (not XRnW2);		-- on config read  (write direction is from logic to RPi)
	config_read_op  <= '1' when (configReg="0100" or configReg="0101" or configReg="0110") else '0';							-- on config set to PIO, DMA or MSG read
	config_write_op <= '1' when (configReg="0000" or configReg="0001" or configReg="0010" or configReg="0011") else '0';		-- on config set to PIO 1st, PIO other, DMA or MSG write

	rpi_writing_to_fifo   <= '1' when (data_read='1'  and config_read_op='1'  and rpi_strobe='1') else '0';
	rpi_reading_from_fifo <= '1' when (data_write='1' and config_write_op='1' and rpi_strobe='1') else '0';
	
	fifo_data_in <= DATA_RPI when rpi_writing_to_fifo='1' else "00000000";
	fifo_WrEn    <= '1'      when rpi_writing_to_fifo='1' else '0';
	fifo_RdEn    <= '1'      when rpi_reading_from_fifo='1' else '0';

	DATA_RPI <= fifo_data_out when (data_write='1'  and config_write_op='1') else -- RPi can read FIFO data when configured data write and the operation in config is write operations
				"00000011"    when (config_read='1' and configReg="1000") else    -- RPi can read IF type (1 is ACSI, 2 is SCSI, 3 is CART)
				"00000001"    when (config_read='1' and configReg="1001") else    -- RPi can read current version - to determine, if this chip needs update or not
				"ZZZZZZZZ";
				
    XATN <= (not fifo_Empty) when (config_write_op='1') else		-- on write operations - showing not-empty (can get data out of it)
			(not fifo_Full)  when (config_read_op='1') else 		-- on read  operations - showing not-full  (can store data into it)
			'0';

	RPiHandling: process(clk, rpi_strobe, config_write) is
    begin
		if rpi_strobe='1' and config_write='1' then		-- when doing config write and got rpi strobe, store new config
			configReg <= DATA_RPI(3 downto 0);
		end if;
    end process;
	
    -- DataChangedState should help with faster read on ST - you can keep reading data and when this toggles (1->0, 0->1), you've just read new data
    --  RPIisIdle tells if RPi has finished last transfer byte (RPIisIdle='1'), or RPi waits for transfer to happen (RPIisIdle='0')
    DataChanged: process(XRESET, XNEXT) is
    begin
        if(XRESET='0') then                      -- reset from RPI? this flag goes to 0
            DataChangedState <= '0';
            RPIisIdle <= '1';
        elsif rising_edge(XNEXT) then            -- if RPi does starts PIO or DMA transfer, the data has changed
            DataChangedState <= not DataChangedState;
            RPIisIdle <= '0';
        end if;
    end process;

    AccessRequest: process(XRESET, XNEXT, READ_CART_LDS) is
    begin
        if(XRESET='0' or XNEXT='1') then            -- on end of transfer (XRESET='0') or before next byte transfer (XNEXT='1') reset this -- RPi will use this to wait for 1st or any other byte
            XATNState <= '0';
        elsif rising_edge(READ_CART_LDS) then       -- when ST accesses LDS (data read or write), we know that we got 1st or any other byte
            XATNState <= '1';
        end if;

        if(XRESET='0' or READ_CART_LDS='1') then    -- reset from RPI or access to data from ST? reset this flag
            RPIwantsMoreState <= '0';
        elsif rising_edge(XNEXT) then               -- RPi wants next byte to be transfered, set this flag
            RPIwantsMoreState <= '1';
        end if;
    end process;

    st_latch_clock <= READ_CART_LDS and (not XRnW); -- when RPi sets WRITE direction (from ST to RPi) and ST does READ_CART_LDS, then this latch clock goes H
    st_doing_read <=  READ_CART_LDS and      XRnW;  -- when RPi sets READ direction (from RPi to ST) and ST does READ_CART_LDS, then this goes H

    -- 8-bit latch register
    -- latch data from ST on falling edge of st_write_latch, which is CS and ACK
    dataLatch: process(st_latch_clock) is
    begin 
        if (rising_edge(st_latch_clock)) then
            st_data_latched <= ADDR(8 downto 1);
        end if;
    end process;

    -- DATA_ST_LOWER is connected to ST DATA(7 downto 0)
    DATA_ST_LOWER <= DATA_RPI when st_doing_read='1' else   -- on reading data directly from RPi
                     "ZZZZZZZZ";                            -- otherwise don't drive this

    -- status register for ST - DataChangedState toggles every time RPi changes the data (stays the same when data not changed)
    statusReg(2) <= RPIisIdle;          -- when H, RPi doesn't do any further transfer (last byte was status byte)
    statusReg(1) <= RPIwantsMoreState;  -- H when ST should transfer another byte (read or write)
    statusReg(0) <= DataChangedState;   -- H when RPi changed data

    -- DATA_ST_UPPER is connected to ST DATA(9 downto 8)
    DATA_ST_UPPER <= statusReg when READ_CART_UDS='1' else  -- UD - status
                     "ZZZ";

    -- DATA_RPI is connected to RPi, data goes out when going from ST to MCU (WRITE operation)
    DATA_RPI <= st_data_latched when XRnW='0'   else  -- on writing data from ST to RPi
                "ZZZZZZZZ";                           -- otherwise don't drive this

end Behavioral;
