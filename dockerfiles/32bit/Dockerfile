FROM nfnty/arch-mini

RUN echo -e "[archlinuxfr]\nSigLevel = Never\nServer = http://repo.archlinux.fr/\$arch" >> /etc/pacman.conf
RUN sed -i '/#\[multilib\]/,/#Include = \/etc\/pacman.d\/mirrorlist/ s/#//' /etc/pacman.conf
RUN pacman -Syyu --noconfirm
RUN pacman -S --noconfirm git curl jshon expac yajl wget unzip cmake
RUN pacman -S --noconfirm --needed base-devel yaourt

RUN useradd -mg root travis
RUN usermod -aG wheel travis
RUN sed -i 's/# %wheel ALL=(ALL) NOPASSWD: ALL/%wheel ALL=(ALL) NOPASSWD: ALL/g' /etc/sudoers

WORKDIR /tmp

RUN pacman -S --noconfirm gcc-multilib
#RUN pacman -R --noconfirm gcc
USER travis
RUN yaourt -S --noconfirm lib32-jansson lib32-curl lib32-sdl2 lib32-sdl2_ttf lib32-speex lib32-fontconfig lib32-openssl
