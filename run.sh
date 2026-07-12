set -e

make -C guest clean all
make -C host clean all

echo ""
echo "Running hypervisor:"
echo ""

./host/build/hypervisor --memory 4 --page 2 --guest guest/build/guest.img