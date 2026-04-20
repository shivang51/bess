// module full_add(a,b,cin,sum,cout);
//   input a,b,cin;
//   output sum,cout;
//   wire x,y,z;
//
//   half_add h1(.a(a),.b(b),.s(x),.c(y));
//   half_add h2(.a(x),.b(cin),.s(sum),.c(z));
//   or o1(cout,y,z);
// endmodule
//
// module half_add(a,b,s,c);
//   input a,b;
//   output s,c;
//
//   xor x1(s,a,b);
//   and a1(c,a,b);
// endmodule 





module gcd_engine (
    input wire clk,
    input wire rst_n,
    input wire start,
    input wire [15:0] data_a,
    input wire [15:0] data_b,
    output reg [15:0] gcd_out,
    output reg done,
    output reg [2:0] current_state
);

    // State Encoding
    localparam IDLE    = 3'b000;
    localparam LOAD    = 3'b001;
    localparam COMPARE = 3'b010;
    localparam SUB_A   = 3'b011;
    localparam SUB_B   = 3'b100;
    localparam FINISH  = 3'b101;

    reg [15:0] a_reg, b_reg;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_state <= IDLE;
            a_reg <= 0;
            b_reg <= 0;
            gcd_out <= 0;
            done <= 0;
        end else begin
            case (current_state)
                IDLE: begin
                    done <= 0;
                    if (start)
                        current_state <= LOAD;
                end

                LOAD: begin
                    a_reg <= data_a;
                    b_reg <= data_b;
                    current_state <= COMPARE;
                end

                COMPARE: begin
                    if (a_reg == b_reg)
                        current_state <= FINISH;
                    else if (a_reg > b_reg)
                        current_state <= SUB_A;
                    else
                        current_state <= SUB_B;
                end

                SUB_A: begin
                    a_reg <= a_reg - b_reg;
                    current_state <= COMPARE;
                end

                SUB_B: begin
                    b_reg <= b_reg - a_reg;
                    current_state <= COMPARE;
                end

                FINISH: begin
                    gcd_out <= a_reg;
                    done <= 1'b1;
                    current_state <= IDLE;
                end

                default: current_state <= IDLE;
            endcase
        end
    end
endmodule
