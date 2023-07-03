#!/bin/bash

mkdir -p resources
cd resources
T=$(pwd)

wget https://www.openssl.org/source/openssl-1.1.1l.tar.gz --no-check-certificate
wget https://www.python.org/ftp/python/3.7.11/Python-3.7.11.tar.xz
git clone https://gitlab.com/qxcv/ssipp.git
git clone --branch fd-interface-fai-cluster https://gitlab.com/danfis/asnets.git asnets

cd $T
tar xf openssl-1.1.1l.tar.gz
cd openssl-1.1.1l
./config --prefix=$T/py3.7/ssl --openssldir=$T/py3.7/ssl/ssl \
    && make \
    && make install

export LD_LIBRARY_PATH=$T/py3.7/ssl/lib:$T/py3.7/lib:$LD_LIBRARY_PATH

cd $T
tar xf Python-3.7.11.tar.xz
cd Python-3.7.11
./configure --prefix $T/py3.7 --enable-optimizations --with-ssl-default-suites=openssl --with-openssl=$T/py3.7/ssl --enable-shared \
    && make \
    && make install
export LD_LIBRARY_PATH=$T/py3.7/lib:$LD_LIBRARY_PATH

cd $T/ssipp
sed -i "s|linker_flags = \\['-pthread', '-lm', '-fno-lto'\\]|linker_flags = ['-pthread', '-lm', '-fno-lto', '-L$T/py3.7/lib', '-lpython3.7m']|" build.py


cd $T/asnets/asnets
sed -i "s|#'ssipp @.*$|'ssipp @ file://localhost/$T/ssipp',|" setup.py

../../py3.7/bin/python3.7 -m venv asnet-env

echo "" >>asnet-env/bin/activate
echo "export LD_LIBRARY_PATH=$T/py3.7/lib:$LD_LIBRARY_PATH" >>asnet-env/bin/activate
echo "export LD_PRELOAD=$T/py3.7/lib/libpython3.7m.so" >>asnet-env/bin/activate

export PYTHON3_ROOT=$T/py3.7
echo "export PYTHON3_ROOT=$T/py3.7" >>asnet-env/bin/activate

export PATH_TORCH=$T/asnets/asnets/asnet-env/lib/python3.7/site-packages/torch
echo "export PATH_TORCH=$T/asnets/asnets/asnet-env/lib/python3.7/site-packages/torch" >>asnet-env/bin/activate

source asnet-env/bin/activate
pip install --upgrade pip
pip install wheel
# downgrade protobuf for now
pip install protobuf==3.20.*
pip install -e .
pip install torch==1.9.0+cpu -f https://download.pytorch.org/whl/cpu/torch_stable.html

echo "export LD_PRELOAD=$LD_PRELOAD:$T/asnets/asnets/asnet-env/lib/python3.7/site-packages/mdpsim.cpython-37m-x86_64-linux-gnu.so" >>asnet-env/bin/activate
deactivate

