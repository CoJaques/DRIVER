#### Colin Jaques - Labo 2 DRV

# Utilisez la page de manuel de la commande mknod pour en comprendre le fonctionnement. Créez ensuite un fichier virtuel de type caractère avec le même couple majeur/mineur que le fichier /dev/random. Qu’est-ce qui se passe lorsque vous lisez son contenu avec la commande cat ?

J'ai utilisé la commande : 
``` bash
sudo mknod my_random c 1 8
```
Car mon /dev/random avait un couple majeur mineur de 1/8

Lors du cat, mon fichier se comporte comme le fichier /dev/random car il est lié au générateur de nombre aléatoires du noyau linux comme /random.

# Exercice 2 : proc
On peut retrouver cette information car dans le fichier /proc/devices, notre device se trouve dans le groupe : Character devices

# Exercice 3: sysfs
Une fois le périphérique situé dans /sys/class/tty/ttyUSB0/ on peut par exemple retrouvé le driver utilisé à l'aide de readlink /device/driver

Le résultat chez moi est ftdi_sio.

En faisant un lsmod | grep ftdi_sio je retrouve bien mon driver.

Chez moi le module n'a aucune dépendance. (défini à l'aide du lsmod)

# Exercice 4 : empty

Lors de l'ajout du module on retrouve un message "Hello there!" dans dmesg.

Par contre pas de message lors du rmmod.

# Exercice 5  Parrot

Lors de l'insertion du module on peut voir dans dmesg que Parrot ready! est affiché ainsi que la PARROT_CMD_TOGGLE et la PARROT_CMD_ALLCASE
On le retrouve bien dans /proc/devices avec un majeur de 97.

Vu que les fonctions utilisée n'automatise pas la création du fichier dans /sys/class/ il faut le créer à la main.

comme demandé, voici le listing de /dev/mynode :
crw-rw-rw- 1 root root 97, 0 Nov  7 20:37 /dev/mynode

comme demandé voici les test de echo et cat :

```bash
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ echo "Hello World" > /dev/mynode
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ cat /dev/mynode
Hello World
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ gcc ioctl.c -o ioctl_test
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ ./ioctl_test /dev/mynode 11008 0
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ cat /dev/mynode
hELLO wORLD
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ ./ioctl_test /dev/mynode 11008 0
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ cat /dev/mynode                 
Hello World
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ ./ioctl_test /dev/mynode 1074014977 0
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ cat /dev/mynode
HELLO WORLD
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ ./ioctl_test /dev/mynode 1074014977 1
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ cat /dev/mynode                      
hello world
~/Doc/repo/drv24/material/lab_03 lab00 ⇡2 !1 ?31 ❯ 
```

Voici un résumé des modifications apportées pour améliorer et moderniser le code du driver `parrot` :

### 1. **Remplacement de `register_chrdev` par `alloc_chrdev_region`**
   - **Objectif** : Permettre une allocation dynamique du numéro majeur pour éviter les conflits avec d'autres drivers.
   - **Modification** : Utilisation de `alloc_chrdev_region` pour obtenir un numéro majeur dynamique et stocker le numéro de périphérique dans `dev_num`.

### 2. **Utilisation de `cdev` pour gérer le périphérique**
   - **Objectif** : Conformité avec les standards modernes de développement de drivers Linux.
   - **Modification** : Initialisation d'une structure `cdev` avec `cdev_init`, puis ajout dans le noyau avec `cdev_add`.

### 3. **Création automatique de `/dev/parrot` avec les permissions 0666**
   - **Objectif** : Éviter la création manuelle de l'entrée dans `/dev` et permettre à tous les utilisateurs de lire et écrire dans le périphérique sans `sudo`.
   - **Modification** : 
     - Création d'une `class` avec `class_create`.
     - Ajout de la fonction `parrot_uevent` pour définir les permissions à `0666`.
     - Affectation de `parrot_uevent` à `parrot_class->dev_uevent`.
     - Création du périphérique `/dev/parrot` avec `device_create`.

### 4. **Ajout de la commande `ioctl` `PARROT_CMD_RESET`**
   - **Objectif** : Permettre de réinitialiser le contenu du périphérique à son état original (comme écrit initialement par l'utilisateur).
   - **Modification** : 
     - Ajout d'un nouveau buffer `original_buffer` pour stocker la chaîne écrite par l'utilisateur.
     - Copie du contenu de `original_buffer` dans `global_buffer` lorsque la commande `PARROT_CMD_RESET` est appelée dans `parrot_ioctl`.

### 5. **Gestion de la mémoire et libération des ressources**
   - **Objectif** : Éviter les fuites de mémoire.
   - **Modification** : Libération de `global_buffer` et `original_buffer` dans `parrot_exit`, ainsi que destruction de la classe, du périphérique et désenregistrement du numéro de périphérique.
