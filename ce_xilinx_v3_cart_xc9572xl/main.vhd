library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity main is
    Port ( 
        -- signals connected to RPI
           STdidTransfer : out std_logic;       -- goes H on 1st byte or any other bytes from ST
           XNEXT         : in std_logic;        -- RPi tells ST that it wants more data
           XRESET        : in std_logic;        -- RPi sets CPLD to idle state, ST will know that no more data should be transfered
           XRnW          : in std_logic;        -- defines data direction (1 (READ): DATA1 <- DATA2 (from RPi to ST),  0 (WRITE): DATA1 -> DATA2 (from ST to RPi))

        -- DATA_ST_LOWER and DATA_ST_UPPER is connected to cartridge port, DATA_RPI connects to RPI and is driven when XRnW is L
           DATA_ST_UPPER: out   std_logic_vector(1 downto 0);
           DATA_ST_LOWER: out   std_logic_vector(7 downto 0);
           DATA_RPI     : inout std_logic_vector(7 downto 0);

        -- ADDR are the address lines from cartridge port
           ADDR    : in std_logic_vector(8 downto 1);

        -- cartridge port signals for accessing the right areas
           LDS     : in std_logic;
           UDS     : in std_logic;
           ROM3    : in std_logic
--         ROM4    : in std_logic
        );
end main;

architecture Behavioral of main is
    signal RPIisIdle        : std_logic;
    signal RPIwantsMoreState: std_logic;
    signal statusReg        : std_logic_vector(1 downto 0);

    signal STdidTransferState: std_logic;

    signal st_data_latched: std_logic_vector(7 downto 0);
    signal st_latch_clock: std_logic;
    signal st_doing_read: std_logic;

    signal READ_CART_LDS : std_logic;
    signal READ_CART_UDS : std_logic;

begin
    READ_CART_LDS <= ((not ROM3) and (not LDS));  -- H when LDS and ROM3 are L
    READ_CART_UDS <= ((not ROM3) and (not UDS));  -- H when UDS and ROM3 are L

    --  RPIisIdle tells if RPi has finished last transfer byte (RPIisIdle='1'), or RPi waits for transfer to happen (RPIisIdle='0')
    DataChanged: process(XRESET, XNEXT) is
    begin
        if(XRESET='0') then                      -- reset from RPI? this flag goes to 0
            RPIisIdle <= '1';
        elsif rising_edge(XNEXT) then            -- if RPi does starts PIO or DMA transfer, the data has changed
            RPIisIdle <= '0';
        end if;
    end process;

    AccessRequest: process(XRESET, XNEXT, READ_CART_LDS) is
    begin
        if(XRESET='0' or XNEXT='1') then            -- on end of transfer (XRESET='0') or before next byte transfer (XNEXT='1') reset this -- RPi will use this to wait for 1st or any other byte
            STdidTransferState <= '0';
        elsif falling_edge(READ_CART_LDS) then      -- when ST accesses LDS (data read or write), we know that we got 1st or any other byte
                                                    -- do this on falling edge of this signal (which is inv. of ROM signal), so the RPi will know it can stop driving data port on READ operations
            STdidTransferState <= '1';
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

    STdidTransfer <= STdidTransferState;    -- if H, RPi can transfer 1st / next byte

    -- DATA_ST_LOWER is connected to ST DATA(7 downto 0)
    DATA_ST_LOWER <= DATA_RPI when st_doing_read='1' else   -- on reading data directly from RPi
                     "ZZZZZZZZ";                            -- otherwise don't drive this

    -- status register for ST
    statusReg(1) <= RPIisIdle;          -- when H, RPi doesn't do any further transfer (last byte was status byte)
    statusReg(0) <= RPIwantsMoreState;  -- H when ST should transfer another byte (read or write)

    -- DATA_ST_UPPER is connected to ST DATA(9 downto 8)
    DATA_ST_UPPER <= statusReg when READ_CART_UDS='1' else  -- UD - status
                     "ZZ";

    -- DATA_RPI is connected to RPi, data goes out when going from ST to MCU (WRITE operation)
    DATA_RPI <= st_data_latched when XRnW='0'   else  -- on writing data from ST to RPi
                "ZZZZZZZZ";                           -- otherwise don't drive this

end Behavioral;
