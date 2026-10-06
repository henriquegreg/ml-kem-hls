PLATFORM := xilinx_u50_gen3x16_xdma_5_202210_1
CONFIG := connectivity.cfg

CONFIG_FLAG := $(if $(wildcard $(CONFIG)),--config $(CONFIG),)

XOS := kyber512_dec.xo kyber768_dec.xo kyber1024_dec.xo
XCLBINS := kyber512_dec.xclbin kyber768_dec.xclbin kyber1024_dec.xclbin

all: $(XCLBINS)
xo: $(XOS)

kyber512_dec.xo:
	KYBER_K_ENV=2 vitis_hls -f run_hls.tcl

kyber768_dec.xo:
	KYBER_K_ENV=3 vitis_hls -f run_hls.tcl

kyber1024_dec.xo:
	KYBER_K_ENV=4 vitis_hls -f run_hls.tcl

%.xclbin: %.xo
	v++ -l -t hw --platform $(PLATFORM) $(CONFIG_FLAG) -s $< -o $@

clean:
	rm -rf *.xo *.xclbin *.log _x .Xil *.jou *.log kyber*_dec