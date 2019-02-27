
# CELESOS - The Most Powerful Infrastructure for Decentralized Applications

Welcome to the CELESOS source code repository! This software enables businesses to rapidly build and deploy high-performance and high-security blockchain-based applications.

Some of the groundbreaking features of CELESOS include:

1. 基于DPOW的共识机制
1. 我们使用燃木作为pow的中间产物
1. ABP是所有被燃木投票选出的BP（出块者）
1. DBP被用来定义所有在主链上的运行DAPP的拥有者
1. 在整个主网中，多签和治理有更重要的位置
1. 提供更丰富的奖励机制给到全网的所有用户，包括开发者和用户
1. 特别提供DBP优惠期用以培育Dapp以及用户
1. 合约可以使用一个特别的权限
1. 购买的ram将会随着时间进行衰减，以减少炒作的概率
1. 如果你没有足够的cpu和net，你将在主网上有一个最低的免费资源可以使用。

CELESOS is released under the open source MIT license and is offered “AS IS” without warranty of any kind, express or implied. Any security provided by the CELESOS software depends in part on how it is used, configured, and deployed. CELESOS is built upon many third-party libraries such as Binaryen (Apache License) and WAVM  (BSD 3-clause) which are also provided “AS IS” without warranty of any kind. Without limiting the generality of the foregoing, CELEOS team makes no representation or guarantee that CELESOS or any third-party libraries will perform as intended or will be free of errors, bugs or faulty code. Both may fail in large or small ways that could completely or partially limit functionality or compromise computer systems. If you use or implement CELESOS, you do so at your own risk. In no event will CELESOS team be liable to any party for any damages whatsoever, even if it had been advised of the possibility of damage.  

CELESOS team is neither launching nor operating any initial public blockchains based upon the EOSIO software. This release refers only to version 1.0 of our open source software. We caution those who wish to use blockchains built on CELESOS to carefully vet the companies and organizations launching blockchains based on EOSIO before disclosing any private keys to their derivative software. 

There is no public testnet running currently.

**If you have previously installed CELESOS, please run the `celesos_uninstall` script (it is in the directory where you cloned CELESOS) before downloading and using the binary releases.**

#### Mac OS X Brew Install
```sh
$ brew tap celes-dev/celesos
$ brew install celesos
```
#### Mac OS X Brew Uninstall
```sh
$ brew remove celesos
```
#### Ubuntu 18.04 Debian Package Install
```sh
$ wget https://github.com/celes-dev/celesos/releases/download/v0.9.0/celesos_0.9.0-1-ubuntu-16.04_amd64.deb
$ sudo apt install ./celesos_0.9.0-1-ubuntu-16.04_amd64.deb
```
#### Ubuntu 16.04 Debian Package Install
```sh
$ wget https://github.com/celes-dev/celesos/releases/download/v0.9.0/celesos_0.9.0-1-ubuntu-18.04_amd64.deb
$ sudo apt install ./celesos_0.9.0-1-ubuntu-18.04_amd64.deb
```
#### Mint 18 Debian Package Install
```sh
$ wget https://github.com/celes-dev/celesos/releases/download/v0.9.0/celesos-0.9.0-1.mint18.x86_64.deb
$ sudo apt install ./celesos-0.9.0-1.mint18.x86_64.deb
```
#### Debian Package Uninstall
```sh
$ sudo apt remove celesos
```
#### Centos RPM Package Install
```sh
$ wget https://github.com/celes-dev/celesos/releases/download/v0.9.0/celesos-0.9.0-1.el7.x86_64.rpm
$ sudo yum install ./celesos-0.9.0-1.el7.x86_64.rpm
```
#### Centos RPM Package Uninstall
```sh
$ sudo yum remove celesos
```
#### Fedora RPM Package Install
```sh
$ wget https://github.com/celes-dev/celesos/releases/download/v0.9.0/celesos-0.9.0-1.fc27.x86_64.rpm
$ sudo yum install ./celesos-0.9.0-1.fc27.x86_64.rpm
```
#### Fedora RPM Package Uninstall
```sh
$ sudo yum remove celesos
```
#### AWS RPM Package Install
```sh
$ wget https://github.com/celes-dev/celesos/releases/download/v0.9.0/celesos-0.9.0-1.aws.x86_64.rpm
$ sudo yum install ./celesos-0.9.0-1.aws.x86_64.rpm
```
#### AWS RPM Package Uninstall
```sh
$ sudo yum remove celesos
```

## Supported Operating Systems
CELESOS currently supports the following operating systems:  
1. Amazon 2018.03 and higher
1. Centos 7
1. Fedora 25 and higher (Fedora 27 recommended)
1. Mint 18
1. Ubuntu 16.04 (Ubuntu 16.10 recommended)
1. Ubuntu 18.04
1. MacOS Darwin 10.12 and higher (MacOS 10.13.x recommended)

## Resources
1. [Website](https://www.celesos.com)
1. [Blog](https://medium.com/eosio)
1. [Developer Portal](developers.celesos.com)
1. [StackExchange for Q&A](https://celes.stackexchange.com/)
1. [Community Telegram Group](https://t.me/EOSProject)
1. [Developer Telegram Group](https://t.me/joinchat/EaEnSUPktgfoI-XPfMYtcQ)
1. [White Paper](https://github.com/EOSIO/Documentation/blob/master/TechnicalWhitePaper.md)
1. [Roadmap](https://github.com/EOSIO/Documentation/blob/master/Roadmap.md)

<a name="gettingstarted"></a>
## Getting Started
Instructions detailing the process of getting the software, building it, running a simple test network that produces blocks, account creation and uploading a sample contract to the blockchain can be found in [Getting Started](https://developers.celesos.com) on the [CELESOS Developer Portal](https://developers.celesos.com).
