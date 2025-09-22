#!/bin/bash
inject_app() {
  IMG_DIR="$1"
  MOUNT_DIR="system_mount"

  # Dynamically find the rootfs image
  ROOTFS=$(find "$IMG_DIR" -name "*.rootfs.qcomflash" -o -name "rootfs.img" | head -n 1)

  if [[ -z "$ROOTFS" || ! -f "$ROOTFS" ]]; then
    echo "❌ rootfs image not found in $IMG_DIR"
    return 1
  fi

  echo "Injecting into $ROOTFS"
  mkdir -p $MOUNT_DIR
  sudo mount -o loop "$ROOTFS" $MOUNT_DIR

  sudo mkdir -p $MOUNT_DIR/data/vendor/iris_test_app/input
  sudo mkdir -p $MOUNT_DIR/data/vendor/iris_test_app/output
  sudo cp /local/mnt/workspace/manigurr/iris_v4l2_test $MOUNT_DIR/data/vendor/iris_test_app/
  sudo chmod +x $MOUNT_DIR/data/vendor/iris_test_app/iris_v4l2_test

  sudo umount $MOUNT_DIR
  rm -rf $MOUNT_DIR
  pwd
}
