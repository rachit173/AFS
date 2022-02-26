sudo apt install apt-transport-https curl gnupg
curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel.gpg
sudo mv bazel.gpg /etc/apt/trusted.gpg.d/
echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | sudo tee /etc/apt/sources.list.d/bazel.list

sudo apt update
sudo apt install bazel
sudo apt-get install libfuse-dev
sudo apt-get install meson
sudo apt-get install python3-pip
pip3 install pytest
echo 'export PATH="$PATH:$HOME/bin"' >> ~/.bashrc


# Install libfuse
wget https://github.com/libfuse/libfuse/releases/download/fuse-3.10.5/fuse-3.10.5.tar.xz
tar -xvf fuse-3.10.5.tar.xz
cd fuse-3.10.5
mkdir -p build
cd build
meson ..
ninja
sudo python3 -m pytest test/
sudo ninja install
cd ../..