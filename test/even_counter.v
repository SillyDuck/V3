module a (reset, clk, z1);
input reset, clk;
output z1;

reg [2:0]x;

//assign z1 = (x == 256'd1125899906842625);
assign z1 = x[0] & x[1] & x[2];

always @(posedge clk) begin
   if (!reset) begin
      x <= 3'b010;
   end
   else begin
      if (x == 3'b010) begin 
         x <= 3'b001;
      end
      if (x == 3'b001) begin
         x <= 3'b100;
      end
      else begin
         x <= 3'b010;
      end
   end
end
endmodule

