#pragma once

// 注册/刷新 .kpn 文件关联，使资源管理器双击用本程序打开。
void EnsureKpnFileAssociation();

// 注册 keypressernext:// 自定义协议（网页「返回客户端」等）。
void EnsureCustomUrlProtocol();
