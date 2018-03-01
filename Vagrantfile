# -*- mode: ruby -*-
# vi: set ft=ruby :

vbox_version = `VBoxManage --version`

Vagrant.configure("2") do |config|
  config.vm.box = "debian/jessie64"

  config.vm.provider "virtualbox" do |vb|
    vb.memory = "1024"
    if vbox_version.to_f >= 5.0
      vb.customize ["modifyvm", :id, "--paravirtprovider", "kvm"]
    end
  end

  config.vm.synced_folder ".", "/vagrant", type: "virtualbox"

  config.vm.provision "shell", inline: <<-SHELL
    apt-key adv --keyserver keyserver.ubuntu.com --recv E56151BF
    echo "deb http://repos.mesosphere.com/debian jessie main" > /etc/apt/sources.list.d/mesosphere.list

    apt-get update
    apt-get install -y                     \
        cmake                              \
        g++                                \
        libcurl4-nss-dev                   \
        libgtest-dev                       \
        mesos
  SHELL

end
