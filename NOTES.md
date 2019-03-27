# Ideas

- Fade out editor window when idle
- Show commented drum beats/etc. highlighted as time passes  "0xx00xx"
- Switch in new state at beginning of a bar, phrase, etc.
- User can immediately compile, auto compile, at next phrase
- Show extra data as added comments

# Release Checks

- [] Notepad mode by default
- [] Package and reload a standard setup
- [] Low and Hi Res displays
- [] Notepad mode abilities: split, close, etc.  User test.
- [] Initial window sizes/shapes

# Vulkan
Wait inFlightFence
Reset inFlightFence
AcquireNextImageKHR:  wait imageAvailableSemaphore  SO: Waiting for the flip to finish with the semaphore, signalling 
VcSubmitQueue: wait imageAvailableSemaphore, signal renderFinished, signal inFlightFence SO: waiting for the image availability, submitting and signalling that the render is complete
VkQueuePresent: wait for renderFinished before flipping

