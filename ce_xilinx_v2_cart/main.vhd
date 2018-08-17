library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity main is
    Port ( 
        -- signals connected to RPI
           reset_hans    : in std_logic;        -- this is the signal which resets Hans, and it will reset this CPLD too to init it
           XCMD          : out std_logic;       -- will go high on 1st cmd byte from ST
           XDONE         : out std_logic;       -- is high when both INTstate and DRQstate are idle (to check if ST finished the transfer)
           XPIO          : in std_logic;        -- on rising edge will put INT to L
           XDMA          : in std_logic;        -- on rising edge will put DRQ to L
           XRnW          : in std_logic;        -- defines data direction (1 (READ): DATA1 <- DATA2 (from RPi to ST),  0 (WRITE): DATA1 -> DATA2 (from ST to RPi))
           XDataNotStatus: in std_logic;        -- when 1 then RPi reads data register, when 0 then RPi reads status register

        -- DATA1 is connected to cartridge port, DATA1s is 2nd status register on cartridge port, DATA2 is data latched on CS and ACK and connected to RPi
           DATA1   : out std_logic_vector(7 downto 0);
           DATA1s  : out std_logic_vector(1 downto 0);
           DATA2   : inout std_logic_vector(7 downto 0);

        -- ADDR are the address lines from cartridge port
           ADDR    : in std_logic_vector(15 downto 1);

        -- cartridge port signals for accessing the right areas
           LDS     : in std_logic;
           UDS     : in std_logic;
           ROM3    : in std_logic
--         ROM4    : in std_logic
        );
end main;

architecture Behavioral of main is
    signal INTstate  : std_logic;
    signal DRQstate  : std_logic;
    signal CMDstate  : std_logic;
    signal st_data_latched: std_logic_vector(7 downto 0);
    signal st_write_latch: std_logic;
    signal st_transfer: std_logic;
    signal st_1st_cmd_byte: std_logic;

    signal DataChangedState: std_logic;     -- toggles every time RPi changes data (on XPIO or XDMA)
    signal DataIsPIOread: std_logic;        -- is H when the data is PIO READ (last byte - transfer status byte), otherwise L

    signal statusReg : std_logic_vector(7 downto 0);
    signal statusReg2: std_logic_vector(1 downto 0);

    signal OUR_ADDR         : std_logic;
    signal READ_FROM_CART   : std_logic;
    signal READ_FROM_CART2  : std_logic;
    signal cmd_status       : std_logic;
    signal cmd_pio_1st_byte : std_logic;
--  signal cmd_pio_write    : std_logic;
--  signal cmd_pio_read     : std_logic;
--  signal cmd_dma_write    : std_logic;
--  signal cmd_dma_read     : std_logic;
    signal ST_reads_status  : std_logic;
    signal ST_reads_data    : std_logic;
    signal RPI_reads_status : std_logic;
    signal RPI_reads_data   : std_logic;
    signal RPI_changes_data : std_logic;
    
begin
    -- operations from ST side (A12 - A9)
    --   get CPLD status (read)          - 1001
    --   1st CMD byte    (PIO write)     - 0110
    --   other CMD bytes (PIO write)     - 0010
    --   status byte     (PIO read)      - 0011
    --   data out        (DMA write)     - 0000
    --   data in         (DMA read)      - 0001
    -- Mapped as: 
    --   ADDR(12) - 1: read  CPLD status; 0: other operations
    --   ADDR(11) - 1: is 1st CMD byte;   0: other bytes
    --   ADDR(10) - 1: PIO operation;     0: DMA operation
    --   ADDR( 9) - 1: read operation;    0: write operation

    OUR_ADDR        <= '1' when ADDR(15 downto 13)="110" else '0';   -- OUR_ADDR is true, if the highest address bits are '110'
    READ_FROM_CART  <=  (OUR_ADDR and (not ROM3) and (not LDS));     -- when address matches and LDS and ROM3 are low, ST is accessing cartridge port - on lower bits where data is
    READ_FROM_CART2 <=  (OUR_ADDR and (not ROM3) and (not UDS));     -- when address matches and UDS and ROM3 are low, ST is accessing cartridge port - on upper bits where status might be

    cmd_status       <= '1' when ADDR(12 downto 9)="1001" else '0';
    cmd_pio_1st_byte <= '1' when ADDR(12 downto 9)="0110" else '0';
--  cmd_pio_write    <= '1' when ADDR(12 downto 9)="0010" else '0';
--  cmd_pio_read     <= '1' when ADDR(12 downto 9)="0011" else '0';
--  cmd_dma_write    <= '1' when ADDR(12 downto 9)="0000" else '0';
--  cmd_dma_read     <= '1' when ADDR(12 downto 9)="0001" else '0';

    st_write_latch <= not (READ_FROM_CART and (not ADDR(9)) );              -- when ST reads with ADDR(9) low, it's a write operation - falling edge on this one will capture address lines as data
    st_transfer <= READ_FROM_CART and (not cmd_status);                     -- if ST reads from cartridge port and it's not reading status reg, but doing anything else (pio/dma read/write), it's a transfer from/to ST
    st_1st_cmd_byte <= READ_FROM_CART and cmd_pio_1st_byte;                 -- if ST reads from cartridge port and it's sending 1st cmd byte, we should set a flag in status register

    ST_reads_status  <= (READ_FROM_CART and ADDR(9) and      cmd_status);   -- when ST wants to read internal status reg
    ST_reads_data    <= (READ_FROM_CART and ADDR(9) and (not cmd_status));  -- when ST wants to read data register

    RPI_reads_status <= (not XRnW) and (not XDataNotStatus);                -- RPi reads status with XDataNotStatus low
    RPI_reads_data   <= (not XRnW) and (    XDataNotStatus);                -- RPi reads status with XDataNotStatus high

    XDONE <= INTstate and DRQstate;                                         -- if both signals are idle (high), then ST transfer was done
    RPI_changes_data <= XPIO or XDMA;                                       -- if one of these signals is rising, we know that RPi has done something with data (PIO / DMA - read / write)

    -- DataChangedState should help with faster read on ST - do a 1st cmd, this goes to low. On DMA read you can keep reading data and when this toggles (1->0, 0->1), you've just read new data
    DataChanged: process(reset_hans, st_1st_cmd_byte, RPI_changes_data) is
    begin
        if(reset_hans='0' or st_1st_cmd_byte='1') then                      -- reset from RPI or 1st CMD byte from ST? this flag goes to 0
            DataChangedState <= '0';
        elsif rising_edge(RPI_changes_data) then                            -- if RPi does starts PIO or DMA transfer, the data has changed
            DataChangedState <= not DataChangedState;
        end if;
    end process;

    -- when ST is doing DMA read and this flag goes H, the read byte was not data but status byte - last byte in transfer
    DoingPIOread: process(reset_hans, st_1st_cmd_byte, XDMA, XPIO) is
    begin
        if(reset_hans='0' or st_1st_cmd_byte='1' or XDMA='1') then          -- reset from RPI or 1st CMD byte from ST or DMA transfer?
            DataIsPIOread <= '0';
        elsif rising_edge(XPIO) then                                        -- if RPi starts PIO read, this bit is high, otherwise low
            DataIsPIOread <= XRnW;
        end if;
    end process;

    CMDrequest: process(reset_hans, XPIO, XDMA, st_1st_cmd_byte) is
    begin
        if(reset_hans='0' or XPIO='1' or XDMA='1') then                     -- reset from RPI or any PIO/DMA transfer? reset this flag
            CMDstate <= '0';
        elsif rising_edge(st_1st_cmd_byte) then                             -- ST is sending 1st cmd byte - set a flag in status register
            CMDstate <= '1';
        end if;
    end process;

    -- D flip-flop with asynchronous reset 
    -- INT low means RPi wants some PIO transfer
    PIOrequest: process(XPIO, st_transfer, reset_hans) is
    begin
        if (st_transfer = '1' or reset_hans = '0') then -- clear with ST transfer or reset
            INTstate <= '1';
        elsif rising_edge(XPIO) then                    -- set with XPIO rising edge
            INTstate <= '0';
        end if;
    end process;

    -- D flip-flop with asynchronous reset 
    -- DRQ low means RPi wants some DMA transfer
    DMArequest: process(XDMA, st_transfer, reset_hans) is
    begin
        if (st_transfer = '1' or reset_hans = '0') then
            DRQstate <= '1';
        elsif rising_edge(XDMA) then
            DRQstate <= '0';
        end if;
    end process;

    -- 8-bit latch register
    -- latch data from ST on falling edge of st_write_latch, which is CS and ACK
    dataLatch: process(st_write_latch) is
    begin 
        if (falling_edge(st_write_latch)) then
            st_data_latched <= ADDR(8 downto 1);
        end if;
    end process;

    -- this is used on RPi as ATN_Hans, when CMD byte is read
    XCMD <= CMDstate;

    -- create status register, which consists of synchornization flags for ST and RPi
    statusReg(7) <= '0';
    statusReg(6) <= '0';
    statusReg(5) <= '0';
    statusReg(4) <= '0';
    statusReg(3) <= '0';
    statusReg(2) <= CMDstate;   -- 1st cmd byte was just sent by ST, cleared by RPi
    statusReg(1) <= DRQstate;   -- RPi wants to send/receive in DMA mode -- set by RPi, cleared by ST
    statusReg(0) <= INTstate;   -- RPi wants to send/receive in PIO mode -- set by RPi, cleared by ST

    -- DATA1 is connected to Atari ST, data goes out when going from RPi to ST (READ operation)
    DATA1 <=    DATA2     when ST_reads_data='1'   else     -- on reading data directly from RPi
                statusReg when ST_reads_status='1' else     -- on reading interanl status register
                "ZZZZZZZZ";                                 -- otherwise don't drive this

    -- 2nd status register for faster DMA read - DataChangedState toggles every time RPi changes the data (stays the same when data not changed)
    statusReg2(1) <= DataIsPIOread;     -- when H, the read byte was the last byte - ST status byte (transfered using PIO read)
    statusReg2(0) <= DataChangedState;

    DATA1s <=   statusReg2 when READ_FROM_CART2='1' else    -- when ST reads upper data from our range, it will contain 2nd status register
                "ZZ";

    -- DATA2 is connected to RPi, data goes out when going from ST to MCU (WRITE operation)
    DATA2 <=    st_data_latched when RPI_reads_data='1'   else  -- on reading data register
                statusReg       when RPI_reads_status='1' else  -- on reading status register
                "ZZZZZZZZ";                                 -- otherwise don't drive this

end Behavioral;
