# Projekat iz Računarske grafike 2021.

## Uputstvo

Kompilacija standardnom procedurom:

```
mkdir build
cd build
cmake ..
make
```

Kompilacija je testirana na operativnim sistemima Windows 7 (samo sa MSYS2!) i Manjaro Linux. 
Biblioteke glfw, assimp i glm moraju biti instalirane gde CMake može da ih nadje. 
Biblioteke imgui i glad su uključene u ovaj git repository.

Startuje se iz sa:

```
./RG-Projekat
```

Radni folder (current working directory) mora biti `build` folder, da bi program mogao da nadje
neophodne fajlove.

## Implementirane tehnike

* Učitavanje (putem biblioteke assimp) i prikazivanje poznatog test modela palate Sponza
* Kamera sa standardnim WASD kontrolama
* Kompletan Blin-Fong model osvetljenja
* Deferred Rendering
* Do 100 animiranih tačkastih svetala (point lights) bez senki
* Jedan animiran reflektor (spot light) sa senkama
* HDR/Gamma correction/Reinhard tone mapping
* Normal mape, spekular mape
* [Relief Parallax Mapping](https://web.archive.org/web/20190131000650/https://www.sunandblackcat.com/tipFullView.php?topicid=28)
* [Reflective Shadow mapping](https://ericpolman.com/2016/03/17/reflective-shadow-maps/)
* [Raymarched volumetric light](http://www.alexandre-pestana.com/volumetric-lights/)

## Slike

![](ss0.jpg)
![](ss1.jpg)
![](ss2.jpg)
![](ss3.jpg)
![](ss4.jpg)
