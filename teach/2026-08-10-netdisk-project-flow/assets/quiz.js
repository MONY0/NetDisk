document.addEventListener("click", (event) => {
  const trigger = event.target.closest("[data-reveal]");
  if (!trigger) return;
  const target = document.getElementById(trigger.dataset.reveal);
  if (!target) return;
  target.classList.toggle("show");
  trigger.textContent = target.classList.contains("show") ? "收起参考答案" : "检查参考答案";
});

document.addEventListener("click", (event) => {
  const choice = event.target.closest("[data-choice]");
  if (!choice) return;
  const quiz = choice.closest(".quiz");
  const feedback = quiz.querySelector("[data-feedback]");
  const correct = choice.dataset.choice === quiz.dataset.answer;
  feedback.textContent = correct ? "正确。请再用自己的话解释为什么。" : "暂不正确。回到本课的调用链，再判断一次。";
  feedback.style.color = correct ? "#0f6b5d" : "#9f2d35";
});
