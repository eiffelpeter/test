# Install below packages on ubuntu
```
sudo apt update 
sudo apt install git 
sudo apt install python3
pip install pre-commit    
sudo apt install cppcheck
sudo apt install clang-format
```

# clang-format code to LLVM style and ccpcheck
`pre-commit run --all-files`

# Build code to exe file 
`gcc test.c -o test`

# Doxygen to list APIs
```
doxygen -g
doxygen Doxyfile
```
[link](./html/files.html)
