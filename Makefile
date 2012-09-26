
qr_encodeem: qr_encodeem.cpp main.cpp
	g++ -Os main.cpp qr_encodeem.cpp qr_utils.cpp -o qrem
