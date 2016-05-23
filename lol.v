module a (clk, z1);
input clk;
output z1;

reg [2:0]x;

//assign z1 = (x == 256'd1125899906842625);
assign z1 = x[0] & x[1] & x[2];

always @(posedge clk) begin
      if (x == 3'b000) begin 
         x <= 3'b001;
      end
      if (x == 3'b001) begin
         x <= 3'b100;
      end
      else begin
         x <= 3'b000;
      end
end
endmodule

