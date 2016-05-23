module a (reset, clk,i1, z1, z2, z3, z4);
input reset, clk, i1;
output z1, z2, z3, z4;

reg [63:0]x;
reg [63:0]w;
reg [7:0]y;
reg [7:0]z;

wire [7:0] sum;

//assign z1 = (x == 256'd1125899906842625);
assign z1 = x[50];
// & x[0];
//assign z1 = x > w;
assign z2 = !(x <= 8'd200);
assign z3 = (x > 8'd200);
assign z4 = (x == 8'd200) || (y == 8'd200);

always @(posedge clk) begin
   if (!reset) begin
      x <= 8'd1;
      y <= 8'd1;
   end
   else begin
      if (i1) begin 
         x <= x * y;
      end
      if ( z == 50 ) begin
         z <= 1;
         y <= 281;
      end
      if (y == 281) begin
        y = 2;
      end
      z <= z+1;
   end
end
endmodule

