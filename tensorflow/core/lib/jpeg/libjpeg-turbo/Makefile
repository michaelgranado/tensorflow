.PHONY: hello clean



hello: jpeg_mem.cc 
	$(CXX) -std=c++17 $(CFLAGS)  jpeg_mem.cc libjpeg.a -o $@

clean:
	-rm hello
