#pragma once

struct path {
	struct vfsmount *mnt;
	struct dentry *dentry;
};