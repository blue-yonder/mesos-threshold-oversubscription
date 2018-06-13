# -*- mode: ruby -*-
# vi: set ft=ruby :

vbox_version = `VBoxManage --version`

machines = ['stretch']

Vagrant.configure("2") do |config|
  machines.each do |os|
    config.vm.define os do |machine|
      machine.vm.box = "debian/#{os}64"

      machine.vm.provider "virtualbox" do |vb|
        vb.memory = "1024"
        if vbox_version.to_f >= 5.0
          vb.customize ["modifyvm", :id, "--paravirtprovider", "kvm"]
        end
      end

      machine.vm.synced_folder ".", "/vagrant", type: "virtualbox"

      machine.vm.provision "shell", inline: <<-SHELL
        apt-get update
        apt-get install -y dirmngr

        apt-key adv --keyserver keyserver.ubuntu.com --recv E56151BF
        echo "deb http://repos.mesosphere.com/debian #{os} main" > /etc/apt/sources.list.d/mesosphere.list

        apt-get update
        apt-get install -y                     \
            cmake                              \
            g++                                \
            libcurl4-nss-dev                   \
            libgtest-dev                       \
            mesos=1.5.0-2.0.1
      SHELL
    end
  end
end
